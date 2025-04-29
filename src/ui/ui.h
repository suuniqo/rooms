
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
ui_free(ui_t* ui);

extern void
ui_start(ui_t* ui);

extern void
ui_stop(ui_t* ui);

extern void
ui_refresh(ui_t* ui);

extern ui_signal_t
ui_handle_keypress(ui_t* ui, char* msg_buf);

extern void
ui_handle_msg(ui_t* ui, const packet_t* packet);

extern void
ui_toggle_conn(ui_t* ui);

#endif /* !defined(UI_H) */
