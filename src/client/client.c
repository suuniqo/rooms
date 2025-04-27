
#include "client.h"

#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

    pthread_mutex_t mail_lock;
    pthread_mutex_t run_lock;

    volatile sig_atomic_t run;
} threads_t;

typedef struct client_context {
    config_t* config;
    threads_t* threads;
    net_t* net;
    ui_t* ui;
} client_context_t;


/*** sync ***/

static bool
client_locked_isrunning(client_context_t* ctx) {
    bool running;

    pthread_mutex_lock(&ctx->threads->run_lock);
    running = ctx->threads->run;
    pthread_mutex_unlock(&ctx->threads->run_lock);

    return running;
}

static void
client_locked_stop(client_context_t* ctx) {
    pthread_mutex_lock(&ctx->threads->run_lock);
    ctx->threads->run = false;
    pthread_mutex_unlock(&ctx->threads->run_lock);
}

static void
client_locked_enqueue(threads_t* threads, ui_t* ui, packet_t* packet) {
    pthread_mutex_lock(&threads->mail_lock);
    ui_handle_msg(ui, packet);
    pthread_mutex_unlock(&threads->mail_lock);
}

static void
client_locked_refresh(threads_t* threads, ui_t* ui) {
    pthread_mutex_lock(&threads->mail_lock);
    ui_refresh(ui);
    pthread_mutex_unlock(&threads->mail_lock);
}


/*** network ***/

static void
client_send(net_t* net, threads_t* threads, ui_t* ui, packet_t* packet) {
    uint8_t packet_buf[MAX_PACKET_SIZE] = {0};

    packet_seal(packet);
    client_locked_enqueue(threads, ui, packet);

    packet_encode(packet, packet_buf);

    if (packet_sendall(net, packet_buf) < 0) {
        error_log("network err: send");
    }
}

static void
client_recv(net_t* net, threads_t* threads, ui_t* ui) {
    uint8_t packet_buf[MAX_PACKET_SIZE] = {0};
    packet_t packet;

    if (packet_recvall(net, packet_buf) < 0) {
        if (errno != 0) {
            error_log("network err: recv");
        }

        return;
    }

    packet_decode(packet_buf, &packet);
    client_locked_enqueue(threads, ui, &packet);
}

/*** client loops ***/

static void*
client_loop_listener(void* ctx_nullable) {
    client_context_t* ctx = (client_context_t*) ctx_nullable;

    while (client_locked_isrunning(ctx)) {
        client_recv(ctx->net, ctx->threads, ctx->ui);
    }

    return NULL;
}

static void
client_loop_talker(client_context_t* ctx) {
    packet_t packet;

    packet_fill_usrname(&packet, ctx->config->usrname);

    while (client_locked_isrunning(ctx)) {
        ui_signal_t rv = ui_handle_keypress(ctx->ui, packet.msg);

        if (rv == SIGNAL_QUIT) {
            client_locked_stop(ctx);
            break;
        }

        if (rv == SIGNAL_MSG) {
            client_send(ctx->net, ctx->threads, ctx->ui, &packet);
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

    ctx->threads->run = true;

    pthread_mutex_init(&ctx->threads->mail_lock, NULL);
    pthread_mutex_init(&ctx->threads->run_lock, NULL);

    if (pthread_create(&ctx->threads->listener, NULL, &client_loop_listener, ctx) != 0) {
        error_shutdown("threads err: create");
    }
}

static void
client_threads_free(client_context_t* ctx) {
    if (pthread_join(ctx->threads->listener, NULL) != 0) {
        error_shutdown("thread err: join");
    }

    pthread_mutex_destroy(&ctx->threads->mail_lock);
    pthread_mutex_destroy(&ctx->threads->run_lock);

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
    net_shutdown(ctx->net, SHUT_RD);                /* unlock the listener thread calling recv */

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
