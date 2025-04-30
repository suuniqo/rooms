
#if !defined(GUI_H)
#define GUI_H

#include <stdint.h>

#include "../term/term.h"
#include "../printer/printer.h"
#include "../prompt/prompt.h"
#include "../chat/chat.h"

/*** screen ***/

extern void
gui_start(void);

extern void
gui_stop(void);


/*** cursor ***/

extern void
gui_undraw_cursor(printer_t* printer);

extern void
gui_draw_cursor(printer_t* printer, unsigned row, unsigned col);


/*** ui ***/

extern void
gui_draw_frame(printer_t* printer, const term_layout_t* lyt);

extern void
gui_draw_header(printer_t* printer, const term_layout_t* lyt, unsigned status_idx, int64_t tick);

extern void
gui_draw_help(printer_t* printer, const term_layout_t* lyt);

extern void
gui_draw_scrsmall(printer_t* printer, const term_layout_t* lyt);

/*** prompt ***/

extern void
gui_draw_prompt(printer_t* printer, const prompt_t* prompt, const prompt_layout_t* lyt);

extern void
gui_draw_chat(printer_t* printer, const chat_t* chat, const chat_layout_t* chat_lyt, const term_layout_t* term_lyt);

#endif /* !defined(GUI_H) */
