
#include "msg.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../uifmt.h"

#include "../../../input/input.h"
#include "../../../error/error.h"

#define TIME_FORMAT "%H:%M"
#define TIME_FORMAT_SIZE 5

/*** color ***/

static const uint8_t PALETTE[] = {
    67,  102, 137,
    138, 139, 174,
    175, 182, 183,
};

static uint8_t
chat_usrname_color(const char* usrname) {
    unsigned hash = 0;

    for (const char* chr = usrname; *chr != '\0'; ++chr) {
        hash += (unsigned) *chr;
    }

    srand(hash);

    return PALETTE[(size_t)rand() % sizeof(PALETTE)];
}


/*** build type ***/

static void
chat_msg_build_default(chat_msg_t* msg, const packet_t* packet, const char* time) {
    msg->msg_blen = (unsigned) strlen(packet->payld);

    msg->msg_ulen = 0;
    for (unsigned i = 0; i < msg->msg_blen; ++msg->msg_ulen) {
        i += input_utf8_blen((uint8_t)packet->payld[i]);
    }

    msg->header_blen = snprintf(msg->buf, sizeof(msg->buf), "\x1b[1;38;5;%dm%s %sat %s%s%s",
            chat_usrname_color(packet->usrname), packet->usrname,
            FONT_FORMAT(COLOR_DEFAULT, THIN),
            FONT_FORMAT(COLOR_LIGHT, BOLD), time, FONT_FORMAT(COLOR_DEFAULT, THIN));

    memcpy(msg->buf + msg->header_blen, packet->payld, msg->msg_blen + 1);
}

static void
chat_msg_build_whisper(chat_msg_t* msg, const packet_t* packet, const char* time) {
    msg->msg_blen = (unsigned) strlen(packet->payld);

    msg->msg_ulen = 0;
    for (unsigned i = 0; i < msg->msg_blen; ++msg->msg_ulen) {
        i += input_utf8_blen((uint8_t)packet->payld[i]);
    }

    msg->header_blen = snprintf(msg->buf, sizeof(msg->buf), "\x1b[1;38;5;%dm%s %swhispered%s at %s%s%s",
            chat_usrname_color(packet->usrname), packet->usrname,
            FONT_FORMAT(COLOR_LIGHT, BOLD), FONT_FORMAT(COLOR_DEFAULT, THIN),
            FONT_FORMAT(COLOR_LIGHT, BOLD), time, FONT_FORMAT(COLOR_DEFAULT, THIN));

    memcpy(msg->buf + msg->header_blen, packet->payld, msg->msg_blen + 1);
}

static void
chat_msg_build_join(chat_msg_t* msg, const packet_t* packet, const char* time) {
    msg->msg_blen = 0;
    msg->msg_ulen = 0;

    msg->header_blen = snprintf(msg->buf, sizeof(msg->buf), "\x1b[1;38;5;%dm%s %sjoined at %s%s",
            chat_usrname_color(packet->usrname), packet->usrname,
            FONT_FORMAT(COLOR_DEFAULT, THIN), FONT_FORMAT(COLOR_LIGHT, BOLD), time);
}

static void
chat_msg_build_exit(chat_msg_t* msg, const packet_t* packet, const char* time) {
    msg->msg_blen = 0;
    msg->msg_ulen = 0;

    msg->header_blen = snprintf(msg->buf, sizeof(msg->buf), "\x1b[1;38;5;%dm%s %sleft at %s%s",
            chat_usrname_color(packet->usrname), packet->usrname,
            FONT_FORMAT(COLOR_DEFAULT, THIN), FONT_FORMAT(COLOR_LIGHT, BOLD), time);
}

/*** build ***/

void
chat_msg_build(chat_msg_t* msg, const packet_t* packet) {
    char time[TIME_FORMAT_SIZE + 1];
    struct tm* tm_info = localtime((const time_t*)&packet->timestamp);
    strftime(time, sizeof(time), TIME_FORMAT, tm_info);

    switch (packet->flags) {
        case PACKET_FLAG_MSG:
            chat_msg_build_default(msg, packet, time);
            break;
        case PACKET_FLAG_WHSP:
            chat_msg_build_whisper(msg, packet, time);
            break;
        case PACKET_FLAG_JOIN:
            chat_msg_build_join(msg, packet, time);
            break;
        case PACKET_FLAG_EXIT:
            chat_msg_build_exit(msg, packet, time);
            break;
        default:
            error_shutdown("msg err: invalid packet flag (%u)", packet->flags);
    }
}
