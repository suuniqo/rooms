#if !defined(MSG_H)
#define MSG_H 

#include <stdbool.h>

#include "../../../../packet/packet.h"


/*** data ***/

#define MSG_HEADER_SIZE 128
#define MSG_WIDTH_RATIO(W) ((W) * 4) / 5

typedef struct {
    char buf[MSG_HEADER_SIZE + SIZE_PAYLD + 1];
    unsigned header_blen;
    unsigned msg_blen;
    unsigned msg_ulen;
} chat_msg_t;


/*** build ***/

void
chat_msg_build(chat_msg_t* msg, const packet_t* packet);

#endif /* !defined(MSG_H) */
