
#if !defined(INPUT_H)
#define INPUT_H

#include <stdbool.h>

#define MAX_UTF8_BYTES 4

#define CNTRL(k) ((k) & 0x1f)

#define RETURN 13
#define ESCAPE 27
#define BACKSPACE 127 

typedef enum {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN,
  INIT_PASTE,
  END_PASTE,
} input_key_t;

extern unsigned
input_utf8_blen(unsigned char first_byte);

extern bool
input_navchr(int c);

extern bool
input_ctrlchr(int c);

extern int
input_read(void);


#endif /* !defined(INPUT_H) */
