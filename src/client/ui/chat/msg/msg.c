
#include "msg.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../uifmt.h"

#include "../../../../input/input.h"
#include "../../../../log/log.h"

#define TIME_FORMAT "%H:%M"
#define TIME_FORMAT_SIZE 5

typedef enum {
    MSG_STATUS_JOIN = 0,
    MSG_STATUS_EXIT,
    MSG_STATUS_DISC,
    MSG_STATUS_LEN,
} msg_status_t;

static const char* const msg_status_str[MSG_STATUS_LEN] = {"joined", "left", "disconnected"};

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
chat_msg_build_status(chat_msg_t* msg, const packet_t* packet, const char* time, msg_status_t st) {
    msg->msg_blen = 0;
    msg->msg_ulen = 0;

    msg->header_blen = snprintf(msg->buf, sizeof(msg->buf), "\x1b[1;38;5;%dm%s %s%s at %s%s",
            chat_usrname_color(packet->usrname), packet->usrname,
            FONT_FORMAT(COLOR_DEFAULT, THIN), msg_status_str[st],
            FONT_FORMAT(COLOR_LIGHT, BOLD), time);
}

/*** build ***/

void
chat_msg_build(chat_msg_t* msg, const packet_t* packet) {
    char time[TIME_FORMAT_SIZE + 1];

    struct tm tm_info;

    if (localtime_r((const time_t*)&packet->timestamp, &tm_info) == NULL) {
        log_shutdown("chat msg err: localtime_r");
    }

    strftime(time, sizeof(time), TIME_FORMAT, &tm_info);

    switch (packet->flags) {
        case PACKET_FLAG_MSG:
            chat_msg_build_default(msg, packet, time);
            break;
        case PACKET_FLAG_WHSP:
            chat_msg_build_whisper(msg, packet, time);
            break;
        case PACKET_FLAG_JOIN:
            chat_msg_build_status(msg, packet, time, MSG_STATUS_JOIN);
            break;
        case PACKET_FLAG_EXIT:
            chat_msg_build_status(msg, packet, time, MSG_STATUS_EXIT);
            break;
        case PACKET_FLAG_DISC:
            chat_msg_build_status(msg, packet, time, MSG_STATUS_DISC);
            break;
        default:
            log_shutdown("msg err: invalid packet flag (%u)", packet->flags);
    }
}
