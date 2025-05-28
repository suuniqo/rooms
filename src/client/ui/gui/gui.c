
#include "gui.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "../uiconf.h"
#include "../uifmt.h"

#include "../../../cleaner/cleaner.h"


const char* const CONN_STR[] = {
    "online",
    "reconn",
};

/*** general ***/

static void
draw_separator(printer_t* printer, unsigned cols, unsigned row)  {
    unsigned len = cols - (2 * SCREEN_PADDING);

    printer_append(printer, "%s\x1b[%d;%dH", FONT_FORMAT(COLOR_DARK, BOLD), row, SCREEN_PADDING + 1);
    for (unsigned i = 0; i < len; ++i) {
        printer_append(printer, "─");
    }
}

static void
draw_scrollable(printer_t* printer, unsigned top, unsigned bottom) {
    printer_append(printer, "%s\x1b[%d;%dr", RESET_SCROLL, top, bottom);
}

/*** cursor ***/

static void
move_cursor(printer_t* printer, unsigned row, unsigned col) {
    printer_append(printer, "\x1b[%d;%dH", row, col);
}


/*** info ***/

#define CTRL_PADDING_RATIO(W) (((W) * 46) / 100)

static void
draw_ctrl(printer_t* printer, const char* key1, const char* ds1, const char* key2, const char* ds2, unsigned win) {
    unsigned ds_padd = CTRL_PADDING_RATIO(win);
    unsigned ctrl_padd = win - (2 * ds_padd);

    printer_append(printer, "%s%s%s\x1b[%uC%s%s\x1b[%uC%s%s\x1b[%uC%s%s\r\n",
            CURSOR_RIGHT(SCREEN_PADDING), FONT_FORMAT(COLOR_DEFAULT, BOLD), key1, ds_padd - strlen(key1) - strlen(ds1), FONT_FORMAT(COLOR_DARK, THIN), ds1,
            ctrl_padd,                    FONT_FORMAT(COLOR_DEFAULT, BOLD), key2, ds_padd - strlen(key2) - strlen(ds2), FONT_FORMAT(COLOR_DARK, THIN), ds2);
}


/*** methods ***/

/* screen */

static void
gui_stop(void) {
    printf("%s%s%s%s%s\n", RESET_SCROLL, FONT_RESET, CURSOR_SHOW, KILL_BRACKETED_PASTE, KILL_ALT_BUF);
}

void
gui_start(void) {
    printf("%s%s\n", INIT_ALT_BUF, INIT_BRACKETED_PASTE);
    cleaner_push((cleaner_fn_t)gui_stop, NULL);
}


/* cursor */

void
gui_undraw_cursor(printer_t* printer) {
    printer_append(printer, "%s", CURSOR_HIDE);
}

void
gui_draw_cursor(printer_t* printer, unsigned row, unsigned col) {
    move_cursor(printer, row, col);
    printer_append(printer, "%s", CURSOR_SHOW);
}


/* ui */

void
gui_draw_frame(printer_t* printer, const term_layout_t* lyt) {
    printer_append(printer, "%s", CLEAR_SCREEN);
    draw_separator(printer, lyt->cols, lyt->rows - PROMPT_HEIGHT + 1);
    draw_scrollable(printer, HEADER_HEIGHT + 1, lyt->rows - PROMPT_HEIGHT);
}

void
gui_draw_header(printer_t* printer, const term_layout_t* lyt, unsigned status_idx, int64_t tick) {
    const char* status = CONN_STR[status_idx];
    const unsigned dots = status_idx == 0 ? 0 : (unsigned)(tick % 4);

    printer_append(printer, "\x1b[2;%dH%s%srooms\x1b[%dC%sstatus %s%s%.*s\x1b[2;%dH%sesc %shelp",
            SCREEN_PADDING + 1, CLEAR_RIGHT,
            FONT_FORMAT(COLOR_LIGHT, BOLD),
            (lyt->cols - (2*SCREEN_PADDING) - (sizeof("status ")-1) - strlen(status) - (sizeof("rooms")-1) - (sizeof("esc help")-1))/2,
            FONT_FORMAT(COLOR_DEFAULT, BOLD), FONT_FORMAT(COLOR_DARK, THIN),
            status, dots, "...", lyt->cols - (sizeof("esc help") - 1) - SCREEN_PADDING,
            FONT_FORMAT(COLOR_DEFAULT, BOLD), FONT_FORMAT(COLOR_DARK, THIN));
}

void
gui_draw_help(printer_t* printer, const term_layout_t* lyt) {
    unsigned row = ((lyt->rows - INFO_HEIGHT - HEADER_HEIGHT) / 2) + HEADER_HEIGHT + 1;
    unsigned win = lyt->cols - (2 * SCREEN_PADDING);

    printer_append(printer, "%s%s", CLEAR_SCREEN, RESET_SCROLL);
    printer_append(printer, "\x1b[%u;%uH%srooms\x1b[%uC%sesc %sclose\n\r\n",
            2, SCREEN_PADDING + 1,                                   FONT_FORMAT(COLOR_LIGHT, BOLD),
            win - (sizeof("rooms") - 1) - (sizeof("esc close") - 1), FONT_FORMAT(COLOR_DEFAULT, BOLD), FONT_FORMAT(COLOR_DARK, THIN));

    printer_append(printer, "\x1b[%u;1H", row);

    draw_separator(printer, lyt->cols, row);
    printer_append(printer, "\n\r\n");                                                  row += 2;

    printer_append(printer, "%s%sediting:\n\r\n",
            CURSOR_RIGHT(SCREEN_PADDING), FONT_FORMAT(COLOR_LIGHT, BOLD));              row += 2;

    draw_ctrl(printer, "ctrl(d)", "del wrd rt", "ctrl(u)", "del upto cursor", win);     row += 1;
    draw_ctrl(printer, "ctrl(w)", "del wrd lt", "ctrl(t)", "del from cursor", win);     row += 2;

    draw_separator(printer, lyt->cols, row);
    printer_append(printer, "\n\r\n");                                                  row += 2;

    printer_append(printer, "%s%snavigation:\n\r\n",
            CURSOR_RIGHT(SCREEN_PADDING), FONT_FORMAT(COLOR_LIGHT, BOLD));              row += 2;

    draw_ctrl(printer, "ctrl(l)", "nav rt", "ctrl(k)", "nav up", win);                  row += 1;
    draw_ctrl(printer, "ctrl(h)", "nav lt", "ctrl(j)", "nav dn", win);                  row += 1;

    printer_append(printer, "\n");                                                      row += 1;

    draw_ctrl(printer, "ctrl(f)", "nav wrd rt", "ctrl(a)", "nav beg", win);             row += 1;
    draw_ctrl(printer, "ctrl(b)", "nav wrd lt", "ctrl(e)", "nav end", win);             row += 2;

    draw_separator(printer, lyt->cols, row);
}

void
gui_draw_scrsmall(printer_t* printer, const term_layout_t* lyt) {
    unsigned col = ((lyt->cols - sizeof("your window is") - 1) / 2) + 1;
    unsigned row = lyt->rows / 2;

    printer_append(printer, "%s", CLEAR_SCREEN);
    printer_append(printer, "\x1b[%d;%dH%syour window is",
            row, col, FONT_FORMAT(COLOR_DEFAULT, THIN));

    if (lyt->rows < MIN_TERM_ROWS) {
        printer_append(printer, "\x1b[%d;%dH%stoo low %sfor %srooms",
                row + 1, col - 1, FONT_BOLD, FONT_FORMAT(COLOR_DEFAULT, THIN), FONT_FORMAT(COLOR_LIGHT, BOLD));
    } else {
        printer_append(printer, "\x1b[%d;%dH%stoo narrow %sfor %srooms",
                row + 1, col - 3, FONT_BOLD, FONT_FORMAT(COLOR_DEFAULT, THIN), FONT_FORMAT(COLOR_LIGHT, BOLD));
    }
}


/* prompt */

void
gui_draw_prompt(printer_t* printer, const prompt_t* prompt, const prompt_layout_t* lyt) {
    move_cursor(printer, lyt->row, SCREEN_PADDING + 1);

    if (prompt_get_blen(prompt) == 0) {
        printer_append(printer, "%smessage%s", FONT_FORMAT(COLOR_DARK, THIN), CLEAR_RIGHT);
    } else {
        printer_append(printer, "%s", FONT_FORMAT(COLOR_DEFAULT, THIN));

        unsigned bytes = prompt_get_visible_blen(prompt, lyt->width);

        printer_append(printer, "%.*s", bytes, prompt_get_visible_start(prompt));
        printer_append(printer, "%s", CLEAR_RIGHT);
    }
}


/* chat */

static void
gui_draw_scrollbar(printer_t* printer, const chat_t* chat, unsigned chat_len, chat_pos_t* top, unsigned msg_count, const chat_layout_t* chat_lyt, const term_layout_t* term_lyt) {
    if (!chat_atbottom(chat)) {
        unsigned scrollbar_pos = (unsigned)roundf((float)((top->msg_idx - msg_count + 1) * chat_lyt->height) / (float)(chat_len - msg_count));

        if (chat_istop(chat, top)) {
            scrollbar_pos = chat_lyt->height;
        } else if (scrollbar_pos == 0) {
            scrollbar_pos = 1;
        }

        printer_append(printer, "\x1b[%u;%uH%s▌", chat_lyt->row - scrollbar_pos, term_lyt->cols, FONT_FORMAT(COLOR_DARK, BOLD));
    }
}

void
gui_draw_chat(printer_t* printer, const chat_t* chat, const chat_layout_t* chat_lyt, const term_layout_t* term_lyt) {
    unsigned chat_len = chat_get_len(chat);

    if (chat_len == 0) {
        return;
    }

    chat_pos_t curr = *chat_get_visible_bottom(chat);
    unsigned filled = chat_expl_up(chat, chat_lyt, &curr);

    unsigned msg_count = curr.chr_idx != 0;
    chat_pos_t top = curr;

    move_cursor(printer, chat_lyt->row, 1);
    printer_append(printer, "%s", FONT_FORMAT(COLOR_DEFAULT, THIN));

    for (unsigned i = 0; i < chat_lyt->height - filled; ++i) {
        printer_append(printer, "\n\r");
    }
 
    for (unsigned i = 0; i < filled; ++i) {
        const chat_msg_t* msg = chat_get_msg(chat, curr.msg_idx);

        assert(curr.chr_idx == 0 || (curr.chr_idx >= msg->header_blen && curr.chr_idx <= msg->header_blen + msg->msg_blen));

        if (curr.chr_idx == msg->header_blen + msg->msg_blen) {
            printer_append(printer, "\n\r");

            curr.msg_idx -= 1;
            curr.chr_idx = 0;
        } else if (curr.chr_idx == 0 && msg->header_blen != 0) {
            printer_append(printer, "%s%.*s\n\r", CURSOR_RIGHT(SCREEN_PADDING), msg->header_blen, msg->buf);

            msg_count += 1;
            curr.chr_idx += msg->header_blen;
        } else {
            unsigned start = curr.chr_idx;
            unsigned bytes = chat_msg_line_dn(chat, chat_lyt, &curr);

            while (msg->buf[start] == ' ') {
                ++start;
            }

            printer_append(printer, "%s%.*s", CURSOR_RIGHT(SCREEN_PADDING), bytes, msg->buf + start);

            const char* end = msg->buf + start + bytes;
            if (*(end - 1) != ' ' && *end != ' ' && *end != '\0') {
                printer_append(printer, "-\n\r");
            } else {
                printer_append(printer, "\n\r");
            }
        }
    }

    gui_draw_scrollbar(printer, chat, chat_len, &top, msg_count, chat_lyt, term_lyt);
}
