

#if !defined(CHAT_H)
#define CHAT_H

#include <stdbool.h>

#include "msg/msg.h"

/*** data ***/

typedef struct {
    unsigned msg_idx;
    unsigned chr_idx;
} chat_pos_t;

typedef struct {
    unsigned width;
    unsigned height;
    unsigned row;
} chat_layout_t;

typedef struct chat chat_t;


/*** methods ***/

extern void
chat_init(chat_t** chat);

extern void
chat_enqueue(chat_t* chat, const packet_t* packet, const chat_layout_t* lyt);

extern void
chat_update(chat_t* chat, const chat_layout_t* lyt);


/*** get ***/

extern const chat_msg_t*
chat_get_msg(const chat_t* chat, unsigned ridx);

extern const chat_pos_t*
chat_get_visible_bottom(const chat_t* chat);

extern unsigned
chat_get_len(const chat_t* chat);

extern bool
chat_atbottom(const chat_t* chat);

extern bool
chat_istop(const chat_t* chat, const chat_pos_t* pos);

extern bool
chat_isbottom(const chat_t* chat, const chat_pos_t* pos);

extern unsigned
chat_msg_line_dn(const chat_t* chat, const chat_layout_t* lyt, chat_pos_t* pos);


/*** move ***/

extern unsigned
chat_expl_up(const chat_t* chat, const chat_layout_t* lyt, chat_pos_t* bottom);


/*** navigation ***/

extern int
chat_nav_up(chat_t* chat, const chat_layout_t* lyt);

extern int
chat_nav_dn(chat_t* chat, const chat_layout_t* lyt);


#endif /* !defined(CHAT_H) */
