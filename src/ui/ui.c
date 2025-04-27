
#include "ui.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "uiconf.h"

#include "cleaner/cleaner.h"
#include "chat/chat.h"
#include "gui/gui.h"
#include "printer/printer.h"
#include "prompt/prompt.h"
#include "term/term.h"

#include "../input/input.h"
#include "../error/error.h"


/*** state ***/

typedef enum {
    EVENT_NONE   = 0,                       /* nothing happened */
    EVENT_PROMPT = 1 << 0,                  /* user interacted with the prompt */
    EVENT_CHAT   = 1 << 1,                  /* user intereacted with the chat or it changed state */
    EVENT_REDRAW = 1 << 2,                  /* every element needs to be updated */
    EVENT_WINCH  = 1 << 3,                  /* user resized screen */
} ui_flag_t;

typedef enum {
    MODE_ROOM,
    MODE_INFO,
    MODE_SMALL,
} ui_mode_t;

typedef struct {
    term_layout_t term_lyt;
    prompt_layout_t prompt_lyt;
    chat_layout_t chat_lyt;
} ui_layout_t;

typedef struct {
    ui_mode_t prev_mode;
    ui_mode_t curr_mode;
    volatile atomic_uchar flags;
} ui_status_t;

typedef struct ui {
    term_t* term;
    chat_t* chat;
    prompt_t* prompt;
    printer_t* printer;
    cleaner_t* cleaner;
    ui_status_t status;
    ui_layout_t layout;
} ui_t;

static ui_t g_ui;


/*** status ***/

/* flags */

#define FLAG_SET(FLAG) (atomic_fetch_or(&g_ui.status.flags, FLAG))
#define FLAG_CLEAR(FLAG) (atomic_fetch_and(&g_ui.status.flags, ~(FLAG)))

#define FLAG_CLEAR_CHECK(FLAG) (FLAG_CLEAR(FLAG) & (FLAG))

inline static bool
flag_set_cond(int rv, ui_flag_t flag) {
    if (rv == 0) {
        (void)FLAG_SET(flag);
    }

    return rv == 0;
}


/* mode */

inline static void
mode_change(ui_mode_t mode) {
    ui_status_t* st = &g_ui.status;

    if (mode != st->curr_mode) {
        st->prev_mode = st->curr_mode;
        st->curr_mode = mode;

        (void)FLAG_SET(EVENT_REDRAW);
    }
}


/* window */

static void
winch_observer(int sig) {
    if (sig == SIGWINCH) {
        (void)FLAG_SET(EVENT_WINCH);
    }
}

static bool
minimum_scrsize(const term_layout_t* lyt) {
    return lyt->rows >= MIN_TERM_ROWS && lyt->cols >= MIN_TERM_COLS;
}

/* status */

static void
status_init(void) {
    g_ui.status = (ui_status_t) {
        .flags = EVENT_WINCH,
        .prev_mode = MODE_ROOM,
        .curr_mode = MODE_ROOM,
    };

    struct sigaction sa;

    sa.sa_handler = winch_observer;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGWINCH, &sa, NULL) == -1) {
        error_shutdown("couldnt setup signals");
    }
}



static void
layout_update(void) {
    unsigned rows = g_ui.layout.term_lyt.rows;
    unsigned cols = g_ui.layout.term_lyt.cols;

    g_ui.layout.prompt_lyt = (prompt_layout_t) {
        .width = cols - (2 * SCREEN_PADDING),
        .row = rows - 1,
    };

    g_ui.layout.chat_lyt = (chat_layout_t) {
        .height = rows - PROMPT_HEIGHT - HEADER_HEIGHT - 1,
        .width = MSG_WIDTH_RATIO(cols - 2 * SCREEN_PADDING),
        .row = rows - PROMPT_HEIGHT,
    };
}

static void
layout_init(void) {
    g_ui.layout.term_lyt = (term_layout_t) {
        .cols = 0,
        .rows = 0,
    };

    (void)term_get_winsize(&g_ui.layout.term_lyt);
    layout_update();
}


/*** update ***/

static void
ui_update(void) {
    const ui_layout_t prev_ui_lyt = g_ui.layout;

    layout_update();

    prompt_update(g_ui.prompt, &g_ui.layout.prompt_lyt, &prev_ui_lyt.prompt_lyt);
    chat_update(g_ui.chat, &g_ui.layout.chat_lyt);
}


/*** refresh ***/

static void
ui_refresh_small(void) {
    const term_layout_t* term_lyt = &g_ui.layout.term_lyt;

    gui_undraw_cursor(g_ui.printer);

    if (FLAG_CLEAR_CHECK(EVENT_REDRAW)) {
        gui_draw_scrsmall(g_ui.printer, term_lyt);
    }
}

static void
ui_refresh_info(void) {
    const term_layout_t* term_lyt = &g_ui.layout.term_lyt;

    gui_undraw_cursor(g_ui.printer);

    if (FLAG_CLEAR_CHECK(EVENT_REDRAW)) {
        gui_draw_info(g_ui.printer, term_lyt);
    }
}

static void
ui_refresh_room(void) {
    const term_layout_t* term_lyt = &g_ui.layout.term_lyt;

    gui_undraw_cursor(g_ui.printer);

    if (FLAG_CLEAR_CHECK(EVENT_REDRAW)) {
        ui_update();
        gui_draw_frame(g_ui.printer, term_lyt);

        FLAG_SET(EVENT_CHAT);
        FLAG_SET(EVENT_PROMPT);
    }

    if (FLAG_CLEAR_CHECK(EVENT_CHAT)) {
        gui_draw_chat(g_ui.printer, g_ui.chat, &g_ui.layout.chat_lyt, term_lyt);
    }

    if (FLAG_CLEAR_CHECK(EVENT_PROMPT)) {
        gui_draw_prompt(g_ui.printer, g_ui.prompt, &g_ui.layout.prompt_lyt);
    }

    unsigned row = g_ui.layout.prompt_lyt.row;
    unsigned col = prompt_get_cursor_col(g_ui.prompt) + SCREEN_PADDING + 1;

    gui_draw_cursor(g_ui.printer, row, col);
}


/*** exit ***/

static void
ui_stop(void) {
    cleaner_run(g_ui.cleaner);
}


/*** methods ***/

void
ui_init(ui_t** ui) {
    *ui = malloc(sizeof(ui_t));

    if (*ui == NULL) {
        error_shutdown("ui err: malloc: no mem for ui");
    }
}

void
ui_free(ui_t* ui) {
    free(ui);
}

void
ui_start(void) {
    cleaner_init(&g_ui.cleaner);
    cleaner_push(g_ui.cleaner, (cleaner_fn_t)cleaner_free, g_ui.cleaner);

    atexit(ui_stop);

    term_init(&g_ui.term);
    cleaner_push(g_ui.cleaner, (cleaner_fn_t)term_free, g_ui.term);

    printer_init(&g_ui.printer);
    cleaner_push(g_ui.cleaner, (cleaner_fn_t)printer_free, g_ui.printer);

    prompt_init(&g_ui.prompt);
    cleaner_push(g_ui.cleaner, (cleaner_fn_t)prompt_free, g_ui.prompt);

    chat_init(&g_ui.chat);
    cleaner_push(g_ui.cleaner, (cleaner_fn_t)chat_free, g_ui.chat);

    status_init();
    layout_init();

    term_enable_rawmode(g_ui.term);
    cleaner_push(g_ui.cleaner, (cleaner_fn_t)term_disable_rawmode, g_ui.term);

    gui_start();
    cleaner_push(g_ui.cleaner, (cleaner_fn_t)gui_stop, NULL);
}

void
ui_refresh(void) {
    if (g_ui.status.flags == EVENT_NONE) {
        return;
    }

    term_layout_t* term_lyt = &g_ui.layout.term_lyt;
    const ui_status_t* status = &g_ui.status;

    if (FLAG_CLEAR_CHECK(EVENT_WINCH)) {
        flag_set_cond(term_get_winsize(term_lyt), EVENT_REDRAW);

        if (!minimum_scrsize(term_lyt)) {
            mode_change(MODE_SMALL);
        } else if (status->curr_mode == MODE_SMALL) {
            mode_change(status->prev_mode);
        }
    }

    switch (status->curr_mode) {
        case MODE_ROOM:
            ui_refresh_room();
            break;
        case MODE_INFO:
            ui_refresh_info();
            break;
        case MODE_SMALL:
            ui_refresh_small();
            break;
    }

    printer_dump(g_ui.printer);
}

ui_signal_t
ui_handle_keypress(char* msg_buf) {
    int key = input_read();

    if (key == 0) {
        return SIGNAL_CONT;
    }

    if (key == INIT_PASTE) {                        /* discard pasted text */
        while (input_read() != END_PASTE) {}
        return SIGNAL_CONT;
    }

    if (g_ui.status.curr_mode == MODE_INFO) {
        if (key == ESCAPE) {
            mode_change(MODE_ROOM);
        }

        return SIGNAL_CONT;
    }

    if (g_ui.status.curr_mode == MODE_SMALL) {
        return SIGNAL_CONT;
    }

    if (input_ctrlchr(key) || input_navchr(key)) {
        if (key == CNTRL('P')) {
            mode_change(MODE_INFO);
            return SIGNAL_CONT;

        }
        if (key == RETURN) {
            return flag_set_cond(prompt_edit_send(g_ui.prompt, msg_buf), EVENT_PROMPT) ? SIGNAL_MSG : SIGNAL_CONT;
        }

        if (key == CNTRL('Q')) {
            return SIGNAL_QUIT;
        }

        int rv;
        ui_flag_t flag;

        switch (key) {
            case ARROW_UP:
            case CNTRL('K'):
                flag = EVENT_CHAT;
                rv = chat_nav_up(g_ui.chat, &g_ui.layout.chat_lyt);
                break;
            case ARROW_DOWN:
            case CNTRL('J'):
                flag = EVENT_CHAT;
                rv = chat_nav_dn(g_ui.chat, &g_ui.layout.chat_lyt);
                break;
            case ARROW_RIGHT:
            case CNTRL('L'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_right(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case ARROW_LEFT:
            case CNTRL('H'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_left(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case DEL_KEY:
            case BACKSPACE:
                flag = EVENT_PROMPT;
                rv = prompt_edit_del(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case PAGE_UP:
            case CNTRL('F'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_right_word(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case PAGE_DOWN:
            case CNTRL('B'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_left_word(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case HOME_KEY:
            case CNTRL('A'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_start(g_ui.prompt);
                break;
            case END_KEY:
            case CNTRL('E'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_end(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case CNTRL('W'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_left_word(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case CNTRL('D'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_right_word(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            case CNTRL('U'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_start(g_ui.prompt);
                break;
            case CNTRL('T'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_end(g_ui.prompt, &g_ui.layout.prompt_lyt);
                break;
            default:
                return SIGNAL_CONT;
                break;
        }

        (void)flag_set_cond(rv, flag);

        return SIGNAL_CONT;
    }


    char chr = (char)key;
    unsigned bytes = input_utf8_blen((uint8_t)chr);

    if (bytes == 0) {                               /* invalid utf8 sequence */
        tcflush(STDIN_FILENO, TCIFLUSH);
        return SIGNAL_CONT;
    }

    char unicode[MAX_UTF8_BYTES] = {chr};
    if (read(STDIN_FILENO, unicode + 1, bytes - 1) == -1) {
        error_shutdown("ui err: couldn't read");
    }

    if (read(STDIN_FILENO, &chr, 1) != 0) {         /* discard multibyte utf8 sequence */
        tcflush(STDIN_FILENO, TCIFLUSH);
        return SIGNAL_CONT;
    }

    flag_set_cond(prompt_edit_write(g_ui.prompt, &g_ui.layout.prompt_lyt, unicode, bytes), EVENT_PROMPT);

    return SIGNAL_CONT;
}

void
ui_handle_msg(const packet_t *packet) {
    chat_enqueue(g_ui.chat, packet, &g_ui.layout.chat_lyt);
    FLAG_SET(EVENT_CHAT);
}
