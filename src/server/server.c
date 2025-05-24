#include "server.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_ssize_t.h>
#include <sys/errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sconn/sconn.h"
#include "sconf/sconf.h"

#include "../error/error.h"
#include "../packet/packet.h"

/*** conf ***/

#define INIT_PFDS_SIZE 5
#define MAX_RESYNCS 8
#define TIMEOUT_MS 200 


/*** data ***/

#define POLLEVS (POLLHUP | POLLERR | POLLNVAL)

typedef enum {
    SHOULD_CONT   =  0,
    SHOULD_KICK   = -1,
    SHOULD_STRIKE = -2,
    SHOULD_RESET  = -3,
} scomm_err_t;


/*** context ***/

typedef struct {
    sconf_t* conf;
    sconn_t* conn;
} scontext_t;

static void
scontext_init(scontext_t* ctx, const char** args) {
    sconf_init(&ctx->conf, args);
    sconn_init(&ctx->conn, ctx->conf->port);
}

static void
scontext_free(scontext_t* ctx) {
    sconn_free(ctx->conn);
    sconf_free(ctx->conf);
}

/*** recv ***/

static scomm_err_t
server_recv(packet_t* packet, int recv_fd) {
    int bytes = packet_recv(packet, recv_fd, MAX_RESYNCS);

    if (bytes == RECV_DISCONN) {
        printf("server: socket %d hung up\n", recv_fd);
    }

    if (bytes == RECV_ERROR) {
        switch (errno) {
            case ETIMEDOUT:
            case ECONNRESET:
                printf("server: socket %d disconnected\n", recv_fd);
                return SHOULD_KICK;
            case EAGAIN:
            case EWOULDBLOCK: /* checked for portability */
                printf("server: socket %d timed out\n", recv_fd);
                return SHOULD_STRIKE;
            default:
                printf("server: socket %d fatal error\n", recv_fd);
                error_log("server err: packet_recv: fatal");
                return SHOULD_KICK;
        }
    }

    if (bytes == RECV_INVAL) {
        error_log("server err: packet_recv: discarded packet");
        return SHOULD_STRIKE;
    }

    if (bytes == RECV_DESYNC) {
        error_log("server err: packet_recv: discarded packet");
        return SHOULD_STRIKE;
    }

    return SHOULD_CONT;
}

/*** respond ***/

static scomm_err_t
server_send(const packet_t* packet, int send_fd) {
    if (packet_send(packet, send_fd) < 0) {
        switch (errno) {
            case ECONNRESET:
            case EPIPE:
            case EAGAIN:
            case EWOULDBLOCK: /* checked for portability */
            case ENETDOWN:
            case ENETUNREACH:
            default:
                break;
        }
    }

    return SHOULD_CONT;
}

static scomm_err_t
server_fanout(const sconn_t* conn, const packet_t* packet, int send_fd) {
    unsigned count = sconn_get_count(conn);
    scomm_err_t rv;

    for (unsigned i = 0; i < count; ++i) {
        struct pollfd pfd = sconn_get_pfd(conn, i);

        if (pfd.fd != conn->listener && pfd.fd != send_fd) {
            if ((rv = server_send(packet, pfd.fd) < 0)) {
                return rv;
            }
        }
    }
}

static scomm_err_t
server_pong(packet_t* packet, int send_fd) {
    packet_build_pong(packet);

    return server_send(packet, send_fd);
}

static scomm_err_t
server_ack(packet_t* packet, int send_fd) {
    packet_build_ack(packet);

    return server_send(packet, send_fd);
}

static scomm_err_t
server_respond(const sconn_t* conn, unsigned fd_idx, packet_t* packet) {
    struct pollfd sender_pfd = sconn_get_pfd(conn, fd_idx);

    scomm_err_t rv = SHOULD_CONT;

    switch ((packet_flags_t)packet->flags) {
        case PACKET_FLAG_MSG:
        case PACKET_FLAG_WHSP:
        case PACKET_FLAG_JOIN:
        case PACKET_FLAG_EXIT:
            rv = server_fanout(conn, packet, sender_pfd.fd);

            if (rv != SHOULD_CONT) {
                break;
            }
            rv = server_ack(packet, sender_pfd.fd);
            break;
        case PACKET_FLAG_PING:
            rv = server_pong(packet, sender_pfd.fd);
            break;
        case PACKET_FLAG_DISC:
        case PACKET_FLAG_ACK:
        case PACKET_FLAG_PONG:
            sconn_kick(conn, fd_idx);
            break;
    }

    return rv;
}


/*** handle ***/

static int
server_handle_client(const sconn_t* conn, unsigned fd_idx) {
    struct pollfd pfd = sconn_get_pfd(conn, fd_idx);

    if (pfd.revents & POLLNVAL) {
        error_shutdown("server err: pollnval on fd %d", pfd.fd);
    }

    if (pfd.revents & POLLERR) {
        int error = 0;
        socklen_t errlen = sizeof(error);
        getsockopt(pfd.fd, SOL_SOCKET, SO_ERROR, &error, &errlen);

        if (error != 0) {
            error_log("server err: pollerr on fd %d: %s", pfd.fd, strerror(error));
        } else {
            error_log("server err: pollerr on fd %d", pfd.fd);
        }

        printf("server: socket %d disconnected\n", pfd.fd);

        sconn_kick(conn, fd_idx);
        return -1;
    }

    if (pfd.revents & POLLHUP) {
        printf("server: socket %d hung up\n", pfd.fd);

        sconn_kick(conn, fd_idx);
        return -1;
    }

    packet_t packet;
    scomm_err_t rv = server_recv(&packet, pfd.fd);
    
    if (rv == SHOULD_STRIKE && sconn_strike(conn, fd_idx) < 0) {
        printf("server: socket %d blocked and kicked\n", pfd.fd);
        sconn_block(conn, fd_idx);
        sconn_kick(conn, fd_idx);
        return -1;
    }

    if (rv == SHOULD_KICK) {
        printf("server: socket %d kicked\n", pfd.fd);
        sconn_kick(conn, fd_idx);
        return -1;
    }

    if (server_respond(conn, fd_idx, &packet) < 0) {
        server_reset(conn);
    }

    return 0;
}


/*** accept ***/

static int
server_accept_client(const sconn_t* conn, unsigned fd_idx) {
    struct pollfd pfd = sconn_get_pfd(conn, fd_idx);

    printf("server: listener was polled\n");

    if (pfd.revents & POLLNVAL) {
        error_shutdown("server err: pollnval on listener");
    }

    if (pfd.revents & POLLERR) {
        int error = 0;
        socklen_t errlen = sizeof(error);
        getsockopt(pfd.fd, SOL_SOCKET, SO_ERROR, &error, &errlen);

        if (error != 0) {
            error_shutdown("server err: pollerr on listener: %s", strerror(error));
        } else {
            error_shutdown("server err: pollerr on listener");
        }
    }

    if (pfd.revents & POLLHUP) {
        error_shutdown("server err: pollhup on listener");
    }

    printf("server: accepting new client...\n");
    return sconn_accept(conn, TIMEOUT_MS);
}


/*** server ***/

void
server_loop(scontext_t* ctx) {
    while (1) {
        int poll_count = sconn_poll(ctx->conn);

        printf("server: polled %d fds\n", poll_count);

        for (unsigned fd_idx = 0; poll_count > 0; ++fd_idx) { // keep going until no more events left 
            printf("server: iter %u\n", fd_idx);
            struct pollfd pfd = sconn_get_pfd(ctx->conn, fd_idx);

            if (!(pfd.revents & (POLLEVS | pfd.events))) {
                continue;
            }

            poll_count -= 1;

            if (pfd.fd == ctx->conn->listener) {
                if (server_accept_client(ctx->conn, fd_idx) < 0) {
                    error_log("server err: couldn't accept client");
                }
                continue;
            }
            
            if (server_handle_client(ctx->conn, fd_idx) < 0) {
                error_log("server err: couldn't handle client");
                fd_idx -= 1;
            }
        }
    }
}

int
server(const char** args) {
    scontext_t ctx;

    scontext_init(&ctx, args);

    server_loop(&ctx);

    scontext_free(&ctx);

    return 0;
}
