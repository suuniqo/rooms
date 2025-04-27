
#include "msg.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../uifmt.h"

#include "../../../input/input.h"

#define TIME_FORMAT "%H:%M"
#define TIME_FORMAT_SIZE 5

/*** color ***/

static const uint8_t PALETTE[] = {
    67,  102, 137,
    138, 139, 174,
    175, 182, 183,
};

static uint8_t
chat_usr_color(const char* usrname) {
    unsigned hash = 0;

    for (const char* chr = usrname; *chr != '\0'; ++chr) {
        hash += (unsigned) *chr;
    }

    srand(hash);

    return PALETTE[(size_t)rand() % sizeof(PALETTE)];
}


/*** build ***/

void
chat_msg_build(chat_msg_t* msg, const packet_t* packet) {
    char time[TIME_FORMAT_SIZE + 1];
    struct tm* tm_info = localtime(&packet->time);
    strftime(time, sizeof(time), TIME_FORMAT, tm_info);
    
    msg->msg_blen = (unsigned) strlen(packet->msg);

    msg->msg_ulen = 0;
    for (unsigned i = 0; i < msg->msg_blen; ++msg->msg_ulen) {
        i += input_utf8_blen((uint8_t)packet->msg[i]);
    }

    msg->header_blen = snprintf(msg->buf, sizeof(msg->buf), "\x1b[1;38;5;%dm%s%s at %s%s%s",
            chat_usr_color(packet->usrname), packet->usrname, FONT_FORMAT(COLOR_DEFAULT, THIN), FONT_FORMAT(COLOR_LIGHT, BOLD), time, FONT_FORMAT(COLOR_DEFAULT, THIN));
    memcpy(msg->buf + msg->header_blen, packet->msg, msg->msg_blen + 1);
}
