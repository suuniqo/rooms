
#include "sconn.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/_types/_timeval.h>
#include <sys/errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pfds/pfds.h"

#include "../../log/log.h"
#include "../../net/net.h"

#define DEFAULT_PORT "9034"
#define INIT_PFDS_SIZE 5


/*** timeval ***/

#define MLS_IN_SEC 1000
#define MCS_IN_MLS 1000

static struct timeval
timeval_from_ms(int ms) {
    struct timeval tm = {
        .tv_sec = ms / MLS_IN_SEC,
        .tv_usec = (ms % MLS_IN_SEC) * MCS_IN_MLS,
    };

    return tm;
}


/*** mem ***/

void
sconn_init(sconn_t** conn_ptr, const char* port) {
    *conn_ptr = malloc(sizeof(sconn_t));

    sconn_t* conn = *conn_ptr;

    if (conn == NULL) {
        log_shutdown("sconn err: malloc");
    }
 
    *conn = (sconn_t) {
        .pfds = pfds_init(INIT_PFDS_SIZE),
        .listener = get_socket_listen(port == NULL ? DEFAULT_PORT : port),
    };

    pfds_join(conn->pfds, conn->listener, POLLIN);
}

void
sconn_free(sconn_t* conn) {
    for (unsigned i = 0; i < conn->pfds->count; ++i) {
        close(conn->pfds->arr[i].fd);
    }

    pfds_free(conn->pfds);
    free(conn);
}


/*** methods ***/

int
sconn_accept(const sconn_t* conn, int timeout_ms) {
    struct sockaddr_storage remoteaddr;
    char remoteIP[INET6_ADDRSTRLEN];
    
    socklen_t addrlen = sizeof(struct sockaddr_storage);
    int newfd = accept(conn->listener, (struct sockaddr*)&remoteaddr, &addrlen);

    if (newfd == -1) {
        log_error("sconn err: accept");
        return -1;
    }

    struct timeval timeout = timeval_from_ms(timeout_ms);

    if (setsockopt(newfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_shutdown("sconn err: failed to set recv timeout: setsockopt");
    }
    if (setsockopt(newfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        log_shutdown("sconn err: failed to set send timeout: setsockopt");
    }

#if defined(__APPLE__) || defined(__MACH__) /* on macOS SIGPIPE has to be disabled manually */
    if (setsockopt(newfd, SOL_SOCKET, SO_NOSIGPIPE, &timeout, sizeof(timeout)) < 0) {
        log_shutdown("sconn err: failed to remove sigpipe: setsockopt");
    }
#endif /* defined(__APPLE__) || defined(__MACH__) */

    if (pfds_join(conn->pfds, newfd, POLLIN)) {
        log_error("sconn err: couldn't accept, server full");
        return -1;
    }

    // TODO remove this and <stdio>, better gui
    printf("server: new connection from %s on socket %d\n",
            inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, sizeof(remoteIP)), newfd);

    return 0;
}

void
sconn_kick(const sconn_t* conn, unsigned idx) {
    close(conn->pfds->arr[idx].fd);
    pfds_exit(conn->pfds, idx);
}

int
sconn_poll(const sconn_t* conn) {
    int poll_count = pfds_poll(conn->pfds);

    if (poll_count < 0) {
        if (errno == EINTR) {
            log_error("sconn err: poll");
            return 0;
        }

        log_shutdown("sconn err: poll");
    }

    return poll_count;
}

unsigned
sconn_get_count(const sconn_t* conn) {
    return conn->pfds->count;
}

struct pollfd
sconn_get_pfd(const sconn_t* conn, unsigned id) {
    return conn->pfds->arr[id];
}

/*** iter ***/


