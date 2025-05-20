
#include "chat.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "msg/msg.h"

#include "../../../error/error.h"
#include "../../../packet/packet.h"
#include "../../../input/input.h"

#define CHAT_HISTORY_SIZE 256

/*** data ***/

typedef struct {
    chat_msg_t buf[CHAT_HISTORY_SIZE];
    unsigned head;
    unsigned len;
} chat_hist_t;

struct chat {
    chat_hist_t hist;
    chat_pos_t scroll;
    bool bottom;
};


/*** aux ***/

static unsigned
chat_prev_pos(unsigned pos) {
    if (pos == 0) {
        return CHAT_HISTORY_SIZE - 1;
    }

    return pos - 1;
}

inline static unsigned
chat_relative_idx(const chat_t* chat, unsigned ridx) {
    return (chat->hist.head + ridx) % CHAT_HISTORY_SIZE;
}


/*** lines ***/

static unsigned
chat_wrd_dn(const chat_t* chat, chat_pos_t* pos) {
    const chat_msg_t* msg = chat_get_msg(chat, pos->msg_idx);

    unsigned wrd_ulen = 0;
    for (wrd_ulen = 0; msg->buf[pos->chr_idx] != '\0' && msg->buf[pos->chr_idx] != ' '; ++wrd_ulen) {
        pos->chr_idx += input_utf8_blen((uint8_t)msg->buf[pos->chr_idx]);
    }

    return wrd_ulen;
}

static int
chat_line_up(const chat_t* chat, const chat_layout_t* lyt, chat_pos_t* pos) {
    const chat_msg_t* msg = chat_get_msg(chat, pos->msg_idx);

    assert(pos->chr_idx == 0 || (pos->chr_idx >= msg->header_blen && pos->chr_idx <= msg->header_blen + msg->msg_blen));

    if (chat_get_len(chat) == 0 || chat_istop(chat, pos)) {
        return -1;
    }

    if (pos->chr_idx == 0) {
        msg = chat_get_msg(chat, pos->msg_idx + 1);

        pos->msg_idx += 1;
        pos->chr_idx = msg->msg_blen + msg->header_blen;

        return 0;
    }


    if (pos->chr_idx == msg->header_blen) {
        pos->chr_idx = 0;

        return 0;
    }

    chat_pos_t temp = (chat_pos_t) {
        .msg_idx = pos->msg_idx,
        .chr_idx = msg->header_blen,
    };

    unsigned prev_idx = temp.chr_idx;

    while (temp.chr_idx != pos->chr_idx) {
        prev_idx = temp.chr_idx;

        chat_msg_line_dn(chat, lyt, &temp);
    }

    pos->chr_idx = prev_idx;

    return 0;
}

static int
chat_line_dn(const chat_t* chat, const chat_layout_t* lyt, chat_pos_t* pos) {
    const chat_msg_t* msg = chat_get_msg(chat, pos->msg_idx);
    
    assert(pos->chr_idx == 0 || (pos->chr_idx >= msg->header_blen && pos->chr_idx <= msg->header_blen + msg->msg_blen));

    if (chat_isbottom(chat, pos)) {
        return -1;
    }

    if (msg->buf[pos->chr_idx] == '\0') {
        pos->msg_idx -= 1;
        pos->chr_idx = 0;

        return 0;
    }

    if (pos->chr_idx == 0 && msg->header_blen != 0) {
        pos->chr_idx = msg->header_blen;
        
        return 0;
    }

    chat_msg_line_dn(chat, lyt, pos);

    return chat_isbottom(chat, pos);
}

static void
chat_rellocate(chat_t* chat, const chat_layout_t* lyt) {
    chat_pos_t* scroll = &chat->scroll;

    chat_pos_t curr = chat->scroll;
    unsigned filled = chat_expl_up(chat, lyt, &curr);

    if (filled < lyt->height) {
        for (unsigned i = 0; i < lyt->height - filled && chat_line_dn(chat, lyt, &chat->scroll) == 0; ++i) {}
    }

    chat->bottom = scroll->msg_idx == 0 && chat_get_msg(chat, 0)->buf[scroll->chr_idx] == '\0';
}

/*** get ***/

const chat_msg_t*
chat_get_msg(const chat_t* chat, unsigned ridx) {
    return &chat->hist.buf[chat_relative_idx(chat, ridx)];
}

const chat_pos_t*
chat_get_visible_bottom(const chat_t* chat) {
    return &chat->scroll;
}

unsigned
chat_get_len(const chat_t* chat) {
    return chat->hist.len;
}

bool
chat_atbottom(const chat_t* chat) {
    return chat->bottom;
}

bool
chat_istop(const chat_t* chat, const chat_pos_t* pos) {
    return pos->msg_idx == chat->hist.len - 1 && pos->chr_idx == 0;
}

bool
chat_isbottom(const chat_t* chat, const chat_pos_t* pos) {
    const chat_msg_t* msg = chat_get_msg(chat, pos->msg_idx);
    return pos->msg_idx == 0 && msg->buf[pos->chr_idx] == '\0';
}


/*** methods ***/

void
chat_init(chat_t** chat) {
    *chat = malloc(sizeof(chat_t));

    if (*chat == NULL) {
        error_shutdown("chat err: no mem");
    }

    **chat = (chat_t) {
        .hist = (chat_hist_t) {
            .head = CHAT_HISTORY_SIZE,
                .len = 0,
        },
            .scroll = (chat_pos_t) {
                .chr_idx = 0,
                .msg_idx = 0,
            },
            .bottom = true,
    };
}

void
chat_free(chat_t* chat) {
    free(chat);
}

void
chat_update(chat_t* chat, const chat_layout_t* lyt) {
    chat_pos_t* scroll = &chat->scroll;

    if (chat_get_len(chat) == 0) {
        return;
    }

    const chat_msg_t* msg = chat_get_msg(chat, scroll->msg_idx);

    if (chat->scroll.chr_idx > msg->header_blen && msg->buf[chat->scroll.chr_idx] != '\0') {
        chat_pos_t temp = (chat_pos_t) {
            .msg_idx = scroll->msg_idx,
            .chr_idx = msg->header_blen,
        };

        while (temp.chr_idx < scroll->chr_idx) {
            chat_line_dn(chat, lyt, &temp);
        }

        *scroll = temp;
    }

    chat_rellocate(chat, lyt);
}

void
chat_enqueue(chat_t* chat, const packet_t *packet, const chat_layout_t* lyt){
    chat_hist_t* hist = &chat->hist;

    hist->head = chat_prev_pos(hist->head);

    if (hist->len < CHAT_HISTORY_SIZE) {
        hist->len += 1;
    }

    chat_msg_t* new = &hist->buf[hist->head];

    chat_msg_build(new, packet);

    if (chat->bottom) {
        chat->scroll.chr_idx = new->header_blen + new->msg_blen;
    } else {
        chat->scroll.msg_idx += 1;

        chat_rellocate(chat, lyt);
    }
}

unsigned
chat_expl_up(const chat_t* chat, const chat_layout_t* lyt, chat_pos_t* bottom) {
    unsigned filled = 0;
    for (; filled < lyt->height && chat_line_up(chat, lyt, bottom) == 0; ++filled) {}

    return filled;
}

unsigned
chat_msg_line_dn(const chat_t* chat, const chat_layout_t* lyt, chat_pos_t* pos) {
    const chat_msg_t* msg = chat_get_msg(chat, pos->msg_idx);

    while (msg->buf[pos->chr_idx] == ' ') {
        ++pos->chr_idx;
    }

    unsigned ulen = 0;
    unsigned first_idx = pos->chr_idx;

    while (ulen < lyt->width && msg->buf[pos->chr_idx] != '\0') {
        unsigned prev_idx = pos->chr_idx;
        unsigned wrd_ulen = chat_wrd_dn(chat, pos);

        if (ulen + wrd_ulen > lyt->width) {
            if (wrd_ulen > lyt->width) {
                pos->chr_idx = prev_idx;

                for (unsigned i = 0; i < lyt->width - ulen; ++i) {
                    pos->chr_idx += input_utf8_blen((uint8_t)msg->buf[pos->chr_idx]);
                }
            } else {
                pos->chr_idx = prev_idx;
            }

            break;
        }

        ulen += wrd_ulen;

        while (msg->buf[pos->chr_idx] == ' ' && ulen < lyt->width) {
            ++pos->chr_idx;
            ++ulen;
        }
    }

    return pos->chr_idx - first_idx;
}


int
chat_nav_up(chat_t* chat, const chat_layout_t* lyt) {
    chat_pos_t curr = chat->scroll;
    unsigned filled = chat_expl_up(chat, lyt, &curr);

    if (filled < lyt->height || (curr.chr_idx == 0 && curr.msg_idx == chat->hist.len - 1)) {
        return -1;
    }

    int status = chat_line_up(chat, lyt, &chat->scroll);

    if (status == 0) {
        chat->bottom = false;
    }

    return status;
}

int
chat_nav_dn(chat_t* chat, const chat_layout_t* lyt) {
    int status = chat_line_dn(chat, lyt, &chat->scroll);

    if (status != 0 && !chat->bottom) {
        chat->bottom = true;
        return 0;
    }

    return status;
}
