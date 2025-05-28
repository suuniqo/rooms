
#include "prompt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../packet/packet.h"
#include "../../../input/input.h"
#include "../../../cleaner/cleaner.h"
#include "../../../log/log.h"


typedef struct {
    unsigned col;
    unsigned bpos;
} prompt_cursor_t;

typedef struct {
    unsigned upos;
    unsigned bpos;
} prompt_scroll_t;

typedef struct {
    char buf[SIZE_PAYLD];
    unsigned blen;
    unsigned ulen;
} prompt_input_t;

struct prompt {
    prompt_input_t input;
    prompt_cursor_t cursor;
    prompt_scroll_t scroll;
};

/*** private ***/

static void
prompt_free(prompt_t* prompt) {
    free(prompt);
}

static unsigned
prompt_prev_blen(const prompt_t* prompt, unsigned bpos) {
    unsigned last_len = 0;

    for (unsigned i = 1; last_len == 0; ++i) {
        last_len = input_utf8_blen((uint8_t)prompt->input.buf[bpos - i]);
    }

    return last_len;
}

static unsigned
prompt_next_blen(const prompt_t* prompt, unsigned bpos) {
    return input_utf8_blen((uint8_t)prompt->input.buf[bpos]);
}

static void
prompt_scroll_left(prompt_t* prompt) {
    prompt_scroll_t* scroll = &prompt->scroll;

    scroll->bpos -= prompt_prev_blen(prompt, scroll->bpos);
    scroll->upos -= 1;
}

static void
prompt_scroll_right(prompt_t* prompt) {
    prompt_scroll_t* scroll = &prompt->scroll;

    scroll->bpos += prompt_next_blen(prompt, scroll->bpos);
    scroll->upos += 1;
}

static int
prompt_trim_whitespace(const prompt_t* prompt, unsigned* start, unsigned* end) {
    const prompt_input_t* input = &prompt->input;

    for (*start = 0; *start < input->blen && input->buf[*start] == ' '; *start += 1) {}

    if (*start == input->blen) {
        return -1;
    }

    for (*end = input->blen - 1; *end > *start && input->buf[*end] == ' '; *end -= 1) {}

    return 0;
}

/*** state ***/

void
prompt_init(prompt_t** prompt) {
    *prompt = malloc(sizeof(prompt_t));

    if (*prompt == NULL) {
        log_shutdown("prompt err: malloc: no mem for prompt");
    }

    **prompt = (prompt_t) {
        .input = (prompt_input_t) {
            .buf = {0},
            .blen = 0,
            .ulen = 0,
        },
        .cursor = (prompt_cursor_t) {
            .col = 0,
            .bpos = 0,
        },
        .scroll = (prompt_scroll_t) {
            .bpos = 0,
            .upos = 0,
        },
    };

    cleaner_push((cleaner_fn_t)prompt_free, (void**)prompt);
}

void
prompt_update(prompt_t* prompt, const prompt_layout_t* lyt, prompt_layout_t prev_lyt) {
    if (prompt->input.blen == 0) {
        return;
    }

    prompt_input_t* input = &prompt->input;
    prompt_scroll_t* scroll = &prompt->scroll;
    prompt_cursor_t* cursor = &prompt->cursor;

    unsigned old_width = prev_lyt.width;

    if (lyt->width <= cursor->col || (lyt->width < old_width && input->ulen > lyt->width)) {
        unsigned iter = old_width - lyt->width;

        for (unsigned i = 0; i < iter && cursor->col > lyt->width / 4; ++i) {
            scroll->bpos += prompt_next_blen(prompt, scroll->bpos);
            scroll->upos += 1;

            cursor->col -= 1;
        }
    } else if (scroll->upos + lyt->width > input->ulen) {
        unsigned iter = scroll->upos + lyt->width - input->ulen;

        for (unsigned i = 0; i < iter && scroll->upos > 0; ++i) {
            scroll->bpos -= prompt_prev_blen(prompt, scroll->bpos);
            scroll->upos -= 1;

            cursor->col += 1;
        }
    }
}


/*** navigation ***/

int
prompt_nav_right(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_input_t* input = &prompt->input;
    prompt_cursor_t* cursor = &prompt->cursor;
    prompt_scroll_t* scroll = &prompt->scroll;

    if (cursor->bpos == input->blen) {
        return -1;
    }

    cursor->bpos += prompt_next_blen(prompt, cursor->bpos);

    if (input->ulen - scroll->upos > lyt->width && cursor->col >= lyt->width * 3 / 4) {
        prompt_scroll_right(prompt);
    } else {
        cursor->col += 1;
    }

    return 0;
}

int
prompt_nav_left(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_cursor_t* cursor = &prompt->cursor;
    prompt_scroll_t* scroll = &prompt->scroll;

    if (cursor->bpos == 0) {
        return -1;
    }

    cursor->bpos -= prompt_prev_blen(prompt, cursor->bpos);

    if (scroll->upos > 0 && cursor->col <= lyt->width / 4) {
        prompt_scroll_left(prompt);
    } else {
        cursor->col -= 1;
    }

    return 0;
}

int
prompt_nav_right_word(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_input_t* input = &prompt->input;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (cursor->bpos == input->blen) {
        return -1;
    }

    while (cursor->bpos != input->blen && input->buf[cursor->bpos] != ' ') {
        prompt_nav_right(prompt, lyt);
    }

    while (cursor->bpos != input->blen && input->buf[cursor->bpos] == ' ') {
        prompt_nav_right(prompt, lyt);
    }

    return 0;
}

int
prompt_nav_left_word(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_input_t* input = &prompt->input;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (cursor->bpos == 0) {
        return -1;
    }

    while (cursor->bpos != 0 && input->buf[cursor->bpos - prompt_prev_blen(prompt, cursor->bpos)] == ' ') {
        prompt_nav_left(prompt, lyt);
    }

    while (cursor->bpos != 0 && input->buf[cursor->bpos - prompt_prev_blen(prompt, cursor->bpos)] != ' ') {
        prompt_nav_left(prompt, lyt);
    }

    return 0;
}

int
prompt_nav_start(prompt_t* prompt) {
    prompt_scroll_t* scroll = &prompt->scroll;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (cursor->bpos == 0) {
        return -1;
    }

    scroll->bpos = 0;
    scroll->upos = 0;

    cursor->bpos = 0;
    cursor->col = 0;

    return 0;
}

int
prompt_nav_end(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_input_t* input = &prompt->input;
    prompt_scroll_t* scroll = &prompt->scroll;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (cursor->bpos == input->blen) {
        return -1;
    }

    cursor->bpos = input->blen;
    cursor->col = lyt->width <= input->ulen ? lyt->width : input->ulen;

    if (lyt->width >= input->ulen) {
        scroll->upos = 0;
        scroll->bpos = 0;
    } else {
        scroll->upos = input->ulen;
        scroll->bpos = input->blen;

        for (unsigned i = 0; i < lyt->width; ++i) {
            scroll->bpos -= prompt_prev_blen(prompt, scroll->bpos);
            scroll->upos -= 1;
        }
    }

    return 0;
}


/*** editing ***/

int
prompt_edit_send(prompt_t* prompt, char* msg_buf) {
    prompt_input_t* input = &prompt->input;
    prompt_cursor_t* cursor = &prompt->cursor;
    prompt_scroll_t* scroll = &prompt->scroll;

    if (input->blen == 0) {
        return -1;
    }

    unsigned start;
    unsigned end;

    if (prompt_trim_whitespace(prompt, &start, &end) != 0) {
        return -1;
    }

    memcpy(msg_buf, input->buf + start, end - start + 1);
    msg_buf[end - start + 1] = '\0';

    input->blen = 0;
    input->ulen = 0;

    cursor->bpos = 0;
    cursor->col = 0;

    scroll->bpos = 0;
    scroll->upos = 0;

    return 0;
}

int
prompt_edit_write(prompt_t* prompt, const prompt_layout_t* lyt, char* unicode, unsigned bytes) {
    prompt_input_t* input = &prompt->input;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (input->blen + bytes > sizeof(input->buf)) {
        return -1;
    }

    if (cursor->bpos != input->blen) {
        memmove(input->buf + cursor->bpos + bytes, input->buf + cursor->bpos, input->blen - cursor->bpos);
    }

    memcpy(input->buf + cursor->bpos, unicode, bytes);

    input->ulen += 1;
    input->blen += bytes;

    prompt_nav_right(prompt, lyt);

    return 0;
}

int
prompt_edit_del(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_input_t* input = &prompt->input;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (cursor->bpos == 0) {
        return -1;
    }

    if (prompt->scroll.upos > 0 && prompt->scroll.upos + lyt->width == input->ulen) {
        cursor->bpos -= prompt_prev_blen(prompt, cursor->bpos);
        prompt_scroll_left(prompt);
    } else {
        prompt_nav_left(prompt, lyt);
    }

    unsigned rm_bytes = prompt_next_blen(prompt, cursor->bpos);

    if (cursor->bpos != input->blen) {
        memmove(input->buf + cursor->bpos, input->buf + cursor->bpos + rm_bytes, input->blen - cursor->bpos);
    }

    input->blen -= rm_bytes;
    input->ulen -= 1;

    return 0;
}

int
prompt_edit_del_left_word(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_input_t* input = &prompt->input;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (cursor->bpos == 0) {
        return -1;
    }

    while (cursor->bpos != 0 && input->buf[cursor->bpos - prompt_prev_blen(prompt, cursor->bpos)] == ' ') {
        prompt_edit_del(prompt, lyt);
    }

    while (cursor->bpos != 0 && input->buf[cursor->bpos - prompt_prev_blen(prompt, cursor->bpos)] != ' ') {
        prompt_edit_del(prompt, lyt);
    }

    return 0;
}

int
prompt_edit_del_right_word(prompt_t* prompt, const prompt_layout_t* lyt) {
    if (prompt_nav_right_word(prompt, lyt) != 0) {
        return -1;
    }

    prompt_edit_del_left_word(prompt, lyt);

    return 0;
}

int
prompt_edit_del_start(prompt_t* prompt) {
    prompt_input_t* input = &prompt->input;
    prompt_scroll_t* scroll = &prompt->scroll;
    prompt_cursor_t* cursor = &prompt->cursor;

    if (cursor->bpos == 0) {
        return -1;
    }

    input->ulen -= scroll->upos + cursor->col;
    input->blen -= cursor->bpos;

    memmove(input->buf, input->buf + cursor->bpos, input->blen);

    cursor->bpos = 0;
    cursor->col = 0;

    scroll->upos = 0;
    scroll->bpos = 0;

    return 0;
}

int
prompt_edit_del_end(prompt_t* prompt, const prompt_layout_t* lyt) {
    prompt_input_t* input = &prompt->input;
    prompt_scroll_t* scroll = &prompt->scroll;
    prompt_cursor_t* cursor = &prompt->cursor;

    input->blen = cursor->bpos;
    input->ulen = scroll->upos + cursor->col;

    for (unsigned i = 0; scroll->upos > 0 && i < lyt->width - cursor->col; ++i) {
        prompt_scroll_left(prompt);
    }

    cursor->col = scroll->upos > 0 ? lyt->width : input->ulen;

    return 0;
}

/*** get ***/

unsigned
prompt_get_cursor_col(const prompt_t* prompt) {
    return prompt->cursor.col;
}

unsigned
prompt_get_blen(const prompt_t* prompt) {
    return prompt->input.blen;
}

unsigned
prompt_get_visible_blen(const prompt_t* prompt, unsigned prompt_width) {
    const prompt_input_t* input = &prompt->input;
    const prompt_scroll_t* scroll = &prompt->scroll;

    unsigned bytes = 0;
    for (unsigned i = 0; i < prompt_width && scroll->bpos + bytes < input->blen; ++i) {
        bytes += input_utf8_blen((uint8_t)input->buf[scroll->bpos + bytes]);
    }

    return bytes;
}

const char*
prompt_get_visible_start(const prompt_t* prompt) {
    return prompt->input.buf + prompt->scroll.bpos;
}
