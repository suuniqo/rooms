
#if !defined(UI_H)
#define UI_H

#include <stdbool.h>

#include "../packet/packet.h"

typedef struct ui ui_t;

typedef enum {
    SIGNAL_MSG,
    SIGNAL_CONT,
    SIGNAL_QUIT,
} ui_signal_t;

extern void
ui_init(ui_t** ui);

extern void
ui_start(void);

extern void
ui_refresh(void);

extern ui_signal_t
ui_handle_keypress(char* msg_buf);

extern void
ui_handle_msg(const packet_t* packet);

#endif /* !defined(UI_H) */
