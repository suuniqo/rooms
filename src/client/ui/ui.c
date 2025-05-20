
#include "ui.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
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

#include "../../input/input.h"
#include "../../syscall/syscall.h"
#include "../../error/error.h"


/*** state ***/

typedef enum {
    EVENT_NONE   = 0,                               /* nothing happened */
    EVENT_PROMPT = 1 << 0,                          /* user interacted with the prompt */
    EVENT_CHAT   = 1 << 1,                          /* user intereacted with the chat or it changed state */
    EVENT_HEADER = 1 << 2,                          /* header changed state */
    EVENT_REDRAW = 1 << 3,                          /* every element needs to be updated */
    EVENT_WINCH  = 1 << 4,                          /* user resized screen */
    EVENT_TICK   = 1 << 5,                          /* a clock tick just happened */
} ui_flag_t;

typedef enum {
    MODE_ROOM,
    MODE_HELP,
    MODE_SMALL,
} ui_mode_t;

typedef enum {
    CONN_ONLINE = 0,
    CONN_OFFLINE,
} ui_conn_t;

typedef struct {
    term_layout_t term_lyt;
    prompt_layout_t prompt_lyt;
    chat_layout_t chat_lyt;
} ui_layout_t;

typedef struct {
    ui_mode_t prev_mode;
    ui_mode_t curr_mode;
    ui_conn_t conn;
} ui_status_t; 

typedef struct {
    int64_t last_tick;
    int64_t delta_time;
    int64_t tick;
} ui_clock_t;

struct ui {
    term_t* term;
    chat_t* chat;
    prompt_t* prompt;
    printer_t* printer;
    cleaner_t* cleaner;
    ui_status_t status;
    ui_layout_t layout;
    ui_clock_t clock;
};

static atomic_uchar status_flags = ATOMIC_VAR_INIT(EVENT_WINCH | EVENT_REDRAW);    /* NOLINT */

#define FLAG_CMP(FLAG)              (atomic_load(&status_flags) == (FLAG))
#define FLAG_SET(FLAG)              (atomic_fetch_or(&status_flags, (FLAG)))
#define FLAG_CLEAR(FLAG)            (atomic_fetch_and(&status_flags, ~(FLAG)))
#define FLAG_TEST_AND_CLEAR(FLAG)   (FLAG_CLEAR(FLAG) & (FLAG))


/*** status ***/

/* flags */

inline static bool
flag_set_cond(int rv, ui_flag_t flag) {
    if (rv == 0) {
        (void)FLAG_SET(flag);
    }

    return rv == 0;
}


/* mode */

inline static void
mode_change(ui_status_t* st, ui_mode_t mode) {
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


/* clock */

static void
clock_init(ui_clock_t* clk) {
    *clk = (ui_clock_t) {
        .delta_time = 0,
        .last_tick = safe_time_ms(),
        .tick = 0,
    };
}

static void
clock_update(ui_clock_t* clk) {
    int64_t now = safe_time_ms();

    clk->delta_time = now - clk->last_tick;

    if (clk->delta_time > TICK_SPEED_MS) {
        clk->last_tick = now;
        clk->delta_time = 0;
        clk->tick += 1;

        FLAG_SET(EVENT_TICK);
    }
}


/* status */

static void
status_init(ui_status_t* st) {
    *st = (ui_status_t) {
        .prev_mode = MODE_ROOM,
        .curr_mode = MODE_ROOM,
        .conn      = CONN_ONLINE,
    };

    struct sigaction sa;

    sa.sa_handler = winch_observer;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGWINCH, &sa, NULL) == -1) {
        error_shutdown("couldnt setup signals");
    }
}


/* layout */

static void
layout_update(ui_layout_t* lyt) {
    unsigned rows = lyt->term_lyt.rows;
    unsigned cols = lyt->term_lyt.cols;

    lyt->prompt_lyt = (prompt_layout_t) {
        .width = cols - (2 * SCREEN_PADDING),
        .row = rows - 1,
    };

    lyt->chat_lyt = (chat_layout_t) {
        .height = rows - PROMPT_HEIGHT - HEADER_HEIGHT - 1,
        .width = MSG_WIDTH_RATIO(cols - 2 * SCREEN_PADDING),
        .row = rows - PROMPT_HEIGHT,
    };
}

static void
layout_init(ui_layout_t* lyt) {
    lyt->term_lyt = (term_layout_t) {
        .cols = 0,
        .rows = 0,
    };

    (void)term_get_winsize(&lyt->term_lyt);
    layout_update(lyt);
}


/*** update ***/

static void
ui_update(ui_t* ui) {
    ui_layout_t lyt_prev = ui->layout;

    layout_update(&ui->layout);

    ui_layout_t* lyt_curr = &ui->layout;

    prompt_update(ui->prompt, &lyt_curr->prompt_lyt, lyt_prev.prompt_lyt);
    chat_update(ui->chat, &lyt_curr->chat_lyt);
}


/*** refresh ***/

static void
ui_refresh_small(ui_t* ui) {
    gui_undraw_cursor(ui->printer);

    if (FLAG_TEST_AND_CLEAR(EVENT_REDRAW)) {
        gui_draw_scrsmall(ui->printer, &ui->layout.term_lyt);
    }
}

static void
ui_refresh_help(ui_t* ui) {
    gui_undraw_cursor(ui->printer);

    if (FLAG_TEST_AND_CLEAR(EVENT_REDRAW)) {
        gui_draw_help(ui->printer, &ui->layout.term_lyt);
    }
}

static void
ui_refresh_room(ui_t* ui) {
    ui_layout_t* lyt = &ui->layout;

    gui_undraw_cursor(ui->printer);

    if (FLAG_TEST_AND_CLEAR(EVENT_REDRAW)) {
        ui_update(ui);
        gui_draw_frame(ui->printer, &lyt->term_lyt);

        FLAG_SET(EVENT_HEADER);
        FLAG_SET(EVENT_CHAT);
        FLAG_SET(EVENT_PROMPT);
    }

    if (FLAG_TEST_AND_CLEAR(EVENT_HEADER)) {
        gui_draw_header(ui->printer, &lyt->term_lyt, ui->status.conn, ui->clock.tick);
    }

    if (FLAG_TEST_AND_CLEAR(EVENT_CHAT)) {
        gui_draw_chat(ui->printer, ui->chat, &lyt->chat_lyt, &lyt->term_lyt);
    }

    if (FLAG_TEST_AND_CLEAR(EVENT_PROMPT)) {
        gui_draw_prompt(ui->printer, ui->prompt, &lyt->prompt_lyt);
    }

    unsigned row = lyt->prompt_lyt.row;
    unsigned col = prompt_get_cursor_col(ui->prompt) + SCREEN_PADDING + 1;

    if (ui->status.conn == CONN_ONLINE) {
        gui_draw_cursor(ui->printer, row, col);
    }
}


/*** keypress ***/

static ui_signal_t
ui_handle_keypress_room(ui_t* ui, int key, char* msg_buf) {
    ui_layout_t* lyt = &ui->layout;
    ui_status_t* st = &ui->status;

    // TODO remove this and manage unsent packets
    if (st->conn != CONN_ONLINE) {
        return SIGNAL_CONT;
    }

    if (input_ctrlchr(key) || input_navchr(key)) {
        if (key == ESCAPE) {
            mode_change(st, MODE_HELP);
            return SIGNAL_CONT;
        }

        if (key == RETURN) {
            if (flag_set_cond(prompt_edit_send(ui->prompt, msg_buf), EVENT_PROMPT)) {
                return  SIGNAL_MSG;
            }

            return SIGNAL_CONT;
        }

        int rv;
        ui_flag_t flag;

        switch (key) {
            case ARROW_UP:
            case CNTRL('K'):
                flag = EVENT_CHAT;
                rv = chat_nav_up(ui->chat, &lyt->chat_lyt);
                break;
            case ARROW_DOWN:
            case CNTRL('J'):
                flag = EVENT_CHAT;
                rv = chat_nav_dn(ui->chat, &lyt->chat_lyt);
                break;
            case ARROW_RIGHT:
            case CNTRL('L'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_right(ui->prompt, &lyt->prompt_lyt);
                break;
            case ARROW_LEFT:
            case CNTRL('H'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_left(ui->prompt, &lyt->prompt_lyt);
                break;
            case DEL_KEY:
            case BACKSPACE:
                flag = EVENT_PROMPT;
                rv = prompt_edit_del(ui->prompt, &lyt->prompt_lyt);
                break;
            case PAGE_UP:
            case CNTRL('F'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_right_word(ui->prompt, &lyt->prompt_lyt);
                break;
            case PAGE_DOWN:
            case CNTRL('B'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_left_word(ui->prompt, &lyt->prompt_lyt);
                break;
            case HOME_KEY:
            case CNTRL('A'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_start(ui->prompt);
                break;
            case END_KEY:
            case CNTRL('E'):
                flag = EVENT_PROMPT;
                rv = prompt_nav_end(ui->prompt, &lyt->prompt_lyt);
                break;
            case CNTRL('W'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_left_word(ui->prompt, &lyt->prompt_lyt);
                break;
            case CNTRL('D'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_right_word(ui->prompt, &lyt->prompt_lyt);
                break;
            case CNTRL('U'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_start(ui->prompt);
                break;
            case CNTRL('T'):
                flag = EVENT_PROMPT;
                rv = prompt_edit_del_end(ui->prompt, &lyt->prompt_lyt);
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

    flag_set_cond(prompt_edit_write(ui->prompt, &lyt->prompt_lyt, unicode, bytes), EVENT_PROMPT);

    return SIGNAL_CONT;

}

static ui_signal_t
ui_handle_keypress_small(void) {
    return SIGNAL_CONT;
}

static ui_signal_t
ui_handle_keypress_help(ui_t* ui, int key) {
    if (key == ESCAPE) {
        mode_change(&ui->status, MODE_ROOM);
    }

    return SIGNAL_CONT;
}


/*** methods ***/

void
ui_init(ui_t** ui) {
    *ui = malloc(sizeof(ui_t));

    if (*ui == NULL) {
        error_shutdown("ui err: malloc:");
    }
}

void
ui_free(ui_t* ui) {
    free(ui);
}

void
ui_stop(ui_t* ui) {
    cleaner_run(ui->cleaner);
}

void
ui_start(ui_t* ui) {
    cleaner_init(&ui->cleaner);
    cleaner_push(ui->cleaner, (cleaner_fn_t)cleaner_free, ui->cleaner);

    term_init(&ui->term);
    cleaner_push(ui->cleaner, (cleaner_fn_t)term_free, ui->term);

    printer_init(&ui->printer);
    cleaner_push(ui->cleaner, (cleaner_fn_t)printer_free, ui->printer);

    prompt_init(&ui->prompt);
    cleaner_push(ui->cleaner, (cleaner_fn_t)prompt_free, ui->prompt);

    chat_init(&ui->chat);
    cleaner_push(ui->cleaner, (cleaner_fn_t)chat_free, ui->chat);

    status_init(&ui->status);
    layout_init(&ui->layout);
    clock_init(&ui->clock);

    term_enable_rawmode(ui->term);
    cleaner_push(ui->cleaner, (cleaner_fn_t)term_disable_rawmode, ui->term);

    gui_start();
    cleaner_push(ui->cleaner, (cleaner_fn_t)gui_stop, NULL);
}

void
ui_refresh(ui_t* ui) {
    ui_layout_t* lyt = &ui->layout;
    ui_status_t* st = &ui->status;

    clock_update(&ui->clock);

    if (FLAG_TEST_AND_CLEAR(EVENT_TICK)) {
        if (ui->status.conn == CONN_OFFLINE) {
            FLAG_SET(EVENT_HEADER);
        }
    }

    if (FLAG_CMP(EVENT_NONE)) {
        return;
    }

    if (FLAG_TEST_AND_CLEAR(EVENT_WINCH)) {
        flag_set_cond(term_get_winsize(&lyt->term_lyt), EVENT_REDRAW);

        if (!minimum_scrsize(&lyt->term_lyt)) {
            mode_change(st, MODE_SMALL);
        } else if (st->curr_mode == MODE_SMALL) {
            mode_change(st, st->prev_mode);
        }
    }

    switch (st->curr_mode) {
        case MODE_ROOM:
            ui_refresh_room(ui);
            break;
        case MODE_HELP:
            ui_refresh_help(ui);
            break;
        case MODE_SMALL:
            ui_refresh_small(ui);
            break;
    }

    printer_dump(ui->printer);
}

ui_signal_t
ui_handle_keypress(ui_t* ui, char* msg_buf) {
    int key = input_read();

    if (key == 0) {                                 /* no input, as input_read is non blocking */
        return SIGNAL_CONT;
    }

    if (key == CNTRL('Q')) {
        return SIGNAL_QUIT;
    }

    if (key == PASTE_INIT) {                        /* discard pasted text */
        while (input_read() != PASTE_END) {}
        return SIGNAL_CONT;
    }

    switch (ui->status.curr_mode) {
        case MODE_ROOM:
            return ui_handle_keypress_room(ui, key, msg_buf);
        case MODE_HELP:
            return ui_handle_keypress_help(ui, key);
        case MODE_SMALL:
            return ui_handle_keypress_small();
    }

    return SIGNAL_CONT;
}

void
ui_handle_msg(ui_t* ui, const packet_t *packet) {
    chat_enqueue(ui->chat, packet, &ui->layout.chat_lyt);
    FLAG_SET(EVENT_CHAT);
}

void
ui_toggle_conn(ui_t *ui) {
    ui_status_t* st = &ui->status;

    st->conn = st->conn == CONN_ONLINE ?
        CONN_OFFLINE : CONN_ONLINE;

    (void)FLAG_SET(EVENT_HEADER);
}
