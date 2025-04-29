
/*** includes ***/

#include "term.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../../error/error.h"
#include "../../syscall/syscall.h"

/*** data ***/

struct term {
    struct termios orig;
};


/*** mem ***/

void
term_init(term_t** term) {
    *term = malloc(sizeof(term_t));

    if (*term == NULL) {
        error_shutdown("term err: malloc: no mem for term");
    }
}

void
term_free(term_t* term) {
    free(term);
}


/*** rawmode ***/

void
term_disable_rawmode(term_t* term) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term->orig) == -1) {
        error_shutdown("term err: couldn't disable raw mode");
    }
}

void
term_enable_rawmode(term_t* term) {
    if (tcgetattr(STDIN_FILENO, &(term->orig)) == -1) {
        error_shutdown("term err: couldn't get terminal attributes");
    }

    struct termios raw_termios = term->orig;

    raw_termios.c_iflag &= (tcflag_t) ~(ICRNL | IXON);
    raw_termios.c_oflag &= (tcflag_t) ~(OPOST);
    raw_termios.c_lflag &= (tcflag_t) ~(ECHO | ICANON | IEXTEN | ISIG);

    raw_termios.c_cc[VMIN] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios) == -1) {
        error_shutdown("term err: couldn't enable raw mode");
    }
}


/*** get ***/

#define POLLING_RETRIES 10

int
term_get_winsize(term_layout_t* lyt) {
    struct winsize prev_ws;
    struct winsize curr_ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &prev_ws) == -1 || prev_ws.ws_col == 0) {
        error_shutdown("term err: couldn't get window size");
    }

    /* In some emulators sigwinch is sent before
     * the terminal visually finishes resizing
     * so polling is necessary to avoid visual bugs
     */

    for (unsigned i = 0; i < POLLING_RETRIES; ++i) { 
        safe_nanosleep(ONE_MS);

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &curr_ws) == -1 || prev_ws.ws_col == 0) {
            error_shutdown("term err: couldn't get window size");
        }

        if (prev_ws.ws_col == curr_ws.ws_col && prev_ws.ws_row == curr_ws.ws_row) {
            break;
        }

        prev_ws = curr_ws;
    }

    if (curr_ws.ws_col == lyt->cols && curr_ws.ws_col == lyt->rows) {
        return -1;
    }

    lyt->cols = curr_ws.ws_col;
    lyt->rows = curr_ws.ws_row;

    return 0;
}
