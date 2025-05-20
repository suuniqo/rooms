
#include "client.h"

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include "ui/ui.h"
#include "conn/cconn.h"
#include "conf/cconf.h"

#include "../error/error.h"
#include "../packet/packet.h"


/*** data ***/

typedef struct threads {
    pthread_t listener;

    pthread_mutex_t lock_ui;
    pthread_mutex_t lock_conn;
    pthread_cond_t cond;

    atomic_bool running;
} threads_t;

typedef struct client_context {
    cconf_t* config;
    threads_t* threads;
    cconn_t* conn;
    ui_t* ui;
} client_context_t;


/*** synchronized ***/

static void
client_locked_enqueue(threads_t* threads, ui_t* ui, packet_t* packet) {
    pthread_mutex_lock(&threads->lock_ui);
    ui_handle_msg(ui, packet);
    pthread_mutex_unlock(&threads->lock_ui);
}

static void
client_locked_refresh(threads_t* threads, ui_t* ui) {
    pthread_mutex_lock(&threads->lock_ui);
    ui_refresh(ui);
    pthread_mutex_unlock(&threads->lock_ui);
}


/*** network ***/

static void
client_send(client_context_t* ctx, packet_t* packet, uint8_t flags) {
    if (packet_send(packet, ctx->conn->sockfd, flags) < 0) {
        error_shutdown("network err: send");
    }

    client_locked_enqueue(ctx->threads, ctx->ui, packet);
}

static void
client_recv(client_context_t* ctx) {
    packet_t packet = {0};

    threads_t* th = ctx->threads;

    int bytes = packet_recv(&packet, ctx->conn->sockfd);

    if (bytes == 0) {                           /* server disconnected */
        ui_toggle_conn(ctx->ui);
        cconn_reconnect(ctx->conn, ctx->config, &th->running, &th->cond, &th->lock_conn);
        ui_toggle_conn(ctx->ui);

        return;
    }

    if (bytes == -1) {
        switch (errno) {
            case ETIMEDOUT:
            case ECONNRESET:
                error_log("network err: recv");
                ui_toggle_conn(ctx->ui);
                cconn_reconnect(ctx->conn, ctx->config, &th->running, &th->cond, &th->lock_conn);
                ui_toggle_conn(ctx->ui);
                break;
            default:
                error_shutdown("network err: recv");
                break;
        }

        return;
    }

    if (bytes == -2) {
        error_log("network err: discarded packet");
        return;
    }

    client_locked_enqueue(ctx->threads, ctx->ui, &packet);
}


/*** stopping ***/

static void
client_stop_listener(threads_t* th, cconn_t* conn) {
    pthread_mutex_lock(&th->lock_conn);

    cconn_shutdown(conn, SHUT_RD);                /* unlock the listener thread calling recv */
    atomic_store(&th->running, false);
    pthread_cond_signal(&th->cond);

    pthread_mutex_unlock(&th->lock_conn);
}


/*** client loops ***/

static void*
client_loop_listener(void* ctx_nullable) {
    client_context_t* ctx = (client_context_t*) ctx_nullable;

    while (atomic_load(&ctx->threads->running)) {
        client_recv(ctx);
    }

    return NULL;
}

static void
client_loop_talker(client_context_t* ctx) {
    packet_t packet = packet_build(ctx->config->usrname);

    client_send(ctx, &packet, PACKET_FLAG_JOIN);

    while (atomic_load(&ctx->threads->running)) {
        ui_signal_t rv = ui_handle_keypress(ctx->ui, packet.payld);

        if (rv == SIGNAL_QUIT) {
            client_send(ctx, &packet, PACKET_FLAG_EXIT);
            client_stop_listener(ctx->threads, ctx->conn);
            break;
        }

        if (rv == SIGNAL_MSG) {
            client_send(ctx, &packet, PACKET_FLAG_MSG);
        }

        client_locked_refresh(ctx->threads, ctx->ui);
    }
}

/*** threads ***/

static void
client_threads_init(client_context_t* ctx) {
    ctx->threads = malloc(sizeof(threads_t));

    if (ctx->threads == NULL) {
        error_shutdown("threads err: malloc");
    }

    ctx->threads->running = ATOMIC_VAR_INIT(true);

    pthread_mutex_init(&ctx->threads->lock_ui, NULL);
    pthread_mutex_init(&ctx->threads->lock_conn, NULL);
    pthread_cond_init(&ctx->threads->cond, NULL);

    if (pthread_create(&ctx->threads->listener, NULL, &client_loop_listener, ctx) != 0) {
        error_shutdown("threads err: create");
    }
}

static void
client_threads_free(client_context_t* ctx) {
    if (pthread_join(ctx->threads->listener, NULL) != 0) {
        error_shutdown("thread err: join");
    }

    pthread_mutex_destroy(&ctx->threads->lock_ui);
    pthread_mutex_destroy(&ctx->threads->lock_conn);
    pthread_cond_destroy(&ctx->threads->cond);

    free(ctx->threads);
    ctx->threads = NULL;
}


/*** context ***/

static void
client_context_init(client_context_t* ctx, const char** args) {
    cconf_init(&ctx->config, args);
    cconn_init(&ctx->conn, ctx->config);
    ui_init(&ctx->ui);
    client_threads_init(ctx);
}

static void
client_context_free(client_context_t* ctx) {
    ui_free(ctx->ui);
    client_threads_free(ctx);
    cconn_free(ctx->conn);
    cconf_free(ctx->config);
}

    
/*** client ***/

int
client(const char** args) {
    client_context_t ctx;

    client_context_init(&ctx, args);

    ui_start(ctx.ui);

    client_loop_talker(&ctx);

    ui_stop(ctx.ui);

    client_context_free(&ctx);

    return 0;
}
