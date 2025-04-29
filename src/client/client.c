
#include "client.h"

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../config/config.h"
#include "../error/error.h"
#include "../net/net.h"
#include "../packet/packet.h"
#include "../ui/ui.h"


/*** data ***/

typedef struct threads {
    pthread_t listener;

    pthread_mutex_t ui_lock;
    pthread_mutex_t net_lock;
    pthread_cond_t cond;

    atomic_bool running;
} threads_t;

typedef struct client_context {
    config_t* config;
    threads_t* threads;
    net_t* net;
    ui_t* ui;
} client_context_t;


static void
client_locked_enqueue(threads_t* threads, ui_t* ui, packet_t* packet) {
    pthread_mutex_lock(&threads->ui_lock);
    ui_handle_msg(ui, packet);
    pthread_mutex_unlock(&threads->ui_lock);
}

static void
client_locked_refresh(threads_t* threads, ui_t* ui) {
    pthread_mutex_lock(&threads->ui_lock);
    ui_refresh(ui);
    pthread_mutex_unlock(&threads->ui_lock);
}


/*** network ***/

static void
client_send(client_context_t* ctx, packet_t* packet) {
    uint8_t packet_buf[MAX_PACKET_SIZE] = {0};

    packet_seal(packet);
    client_locked_enqueue(ctx->threads, ctx->ui, packet);

    packet_encode(packet, packet_buf);

    if (packet_sendall(ctx->net, packet_buf) < 0) {
        error_log("network err: send");
    }
}

static void
client_recv(client_context_t* ctx) {
    uint8_t packet_buf[MAX_PACKET_SIZE] = {0};
    packet_t packet;

    threads_t* th = ctx->threads;

    int bytes = packet_recvall(ctx->net, packet_buf);

    if (bytes == 0) {                           /* server disconnected */
        ui_toggle_conn(ctx->ui);
        net_reconnect(ctx->net, ctx->config, &th->running, &th->cond, &th->net_lock);
        ui_toggle_conn(ctx->ui);

        return;
    }

    if (bytes < 0) {
        switch (errno) {
            case ECONNRESET:
                error_log("network err: recv");

                ui_toggle_conn(ctx->ui);
                net_reconnect(ctx->net, ctx->config, &th->running, &th->cond, &th->net_lock);
                ui_toggle_conn(ctx->ui);
                break;
            default:
                error_shutdown("network err: recv");
                break;
        }

        return;
    }

    if (bytes < MIN_PACKET_SIZE) {              /* the packet is discarded */
        return;
    }

    packet_decode(packet_buf, &packet);
    client_locked_enqueue(ctx->threads, ctx->ui, &packet);
}


/*** stopping ***/

static void
client_stop_listener(threads_t* th, net_t* net) {
    pthread_mutex_lock(&th->net_lock);

    net_shutdown(net, SHUT_RD);                /* unlock the listener thread calling recv */
    atomic_store(&th->running, false);
    pthread_cond_signal(&th->cond);

    pthread_mutex_unlock(&th->net_lock);
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
    packet_t packet;

    packet_fill_usrname(&packet, ctx->config->usrname);

    while (atomic_load(&ctx->threads->running)) {
        ui_signal_t rv = ui_handle_keypress(ctx->ui, packet.msg);

        if (rv == SIGNAL_QUIT) {
            client_stop_listener(ctx->threads, ctx->net);
            break;
        }

        if (rv == SIGNAL_MSG) {
            client_send(ctx, &packet);
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

    pthread_mutex_init(&ctx->threads->ui_lock, NULL);
    pthread_mutex_init(&ctx->threads->net_lock, NULL);
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

    pthread_mutex_destroy(&ctx->threads->ui_lock);
    pthread_mutex_destroy(&ctx->threads->net_lock);
    pthread_cond_destroy(&ctx->threads->cond);

    free(ctx->threads);
    ctx->threads = NULL;
}


/*** context ***/

static void
client_context_init(client_context_t* ctx, char** args) {
    config_init(&ctx->config, args);
    net_init(&ctx->net, ctx->config);
    ui_init(&ctx->ui);
    client_threads_init(ctx);
}

static void
client_context_free(client_context_t* ctx) {
    ui_free(ctx->ui);
    client_threads_free(ctx);
    net_free(ctx->net);
    config_free(ctx->config);
}

    
/*** client ***/

int
client(char** args) {
    client_context_t ctx;

    client_context_init(&ctx, args);

    ui_start(ctx.ui);

    client_loop_talker(&ctx);

    ui_stop(ctx.ui);

    client_context_free(&ctx);

    return 0;
}
