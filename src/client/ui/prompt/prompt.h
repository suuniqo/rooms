
#if !defined(PROMPT_H)
#define PROMPT_H

/*** data ***/

typedef struct {
    unsigned width;
    unsigned row;
} prompt_layout_t;

typedef struct prompt prompt_t;


/*** set ***/

extern void
prompt_init(prompt_t** prompt);

extern void
prompt_update(prompt_t* prompt, const prompt_layout_t* lyt, prompt_layout_t prev_lyt);


/*** get ***/

extern unsigned
prompt_get_cursor_col(const prompt_t* prompt);

extern unsigned
prompt_get_blen(const prompt_t* prompt);

extern unsigned
prompt_get_visible_blen(const prompt_t* prompt, unsigned prompt_width);

const char*
prompt_get_visible_start(const prompt_t* prompt);


/*** navigation ***/

extern int
prompt_nav_right(prompt_t* prompt, const prompt_layout_t* lyt);

extern int
prompt_nav_left(prompt_t* prompt, const prompt_layout_t* lyt);

extern int
prompt_nav_right_word(prompt_t* prompt, const prompt_layout_t* lyt);

extern int
prompt_nav_left_word(prompt_t* prompt, const prompt_layout_t* lyt);

extern int
prompt_nav_start(prompt_t* prompt);

extern int
prompt_nav_end(prompt_t* prompt, const prompt_layout_t* lyt);


/*** editing ***/

extern int
prompt_edit_send(prompt_t* prompt, char* msg_buf);

extern int
prompt_edit_write(prompt_t* prompt, const prompt_layout_t* lyt, char* unicode, unsigned bytes);

extern int
prompt_edit_del(prompt_t* prompt, const prompt_layout_t* lyt);

extern int
prompt_edit_del_left_word(prompt_t* prompt, const prompt_layout_t* lyt);

extern int
prompt_edit_del_right_word(prompt_t* prompt, const prompt_layout_t* lyt);

extern int
prompt_edit_del_start(prompt_t* prompt);

extern int
prompt_edit_del_end(prompt_t* prompt, const prompt_layout_t* lyt);


#endif /* !defined(PROMPT_H) */
