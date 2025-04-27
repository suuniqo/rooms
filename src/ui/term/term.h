
#if !defined(TERM_H)
#define TERM_H


/*** includes ***/

#include <termios.h>

/*** term ***/

typedef struct {
    unsigned rows;
    unsigned cols;
} term_layout_t;

typedef struct term term_t;

/*** mem ***/

extern void
term_init(term_t** term);

extern void
term_free(term_t* term);


/*** rawmode ***/

extern void
term_disable_rawmode(term_t *term_conf);

extern void
term_enable_rawmode(term_t *term_conf);


/*** window ***/

extern int
term_get_winsize(term_layout_t* lyt);


#endif /* !defined(TERM_H) */
