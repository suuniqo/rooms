
#include "input.h"

#include <unistd.h>
#include <errno.h>

#include "../log/log.h"

#define MAX_SEQ_SIZE 5

unsigned
input_utf8_blen(unsigned char first_byte) {
    if ((first_byte & 0x80) == 0x00) {
        return 1;
    }
    
    if ((first_byte & 0xE0) == 0xC0) {
        return 2;
    }
    
    if ((first_byte & 0xF0) == 0xE0) {
        return 3;
    }

    if ((first_byte & 0xF8) == 0xF0) {
        return 4;
    }

    return 0;
}

bool
input_navchr(int c) {
    return c >= ARROW_LEFT && c <= PAGE_DOWN;
}

bool
input_ctrlchr(int c) {
    return (c > '\0' && c < ' ') || c == BACKSPACE;
}

// TODO eventually i should fix this... right?

int
input_read(void) {
    ssize_t nread;
    char c = 0;

    if ((nread = read(STDIN_FILENO, &c, 1)) == -1 && errno != EAGAIN) {
        log_shutdown("read");
    }

    if (c == '\x1b') {
        char seq[MAX_SEQ_SIZE];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) {
            return '\x1b';
        }

        if (read(STDIN_FILENO, &seq[1], 1) != 1) {
            return '\x1b';
        }

        if(seq[0] == '[') {
            if(seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) {
                    return '\x1b';
                }

                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                        default: break;
                    }
                } else if (seq[2] == ';') {
                    if (read(STDIN_FILENO, &seq[3], 2) != 2) {
                        return '\x1b';
                    }

                    switch(seq[4]) {
                        case 'A': return ARROW_UP;
                        case 'B': return ARROW_DOWN; 
                        case 'C': return ARROW_RIGHT;
                        case 'D': return ARROW_LEFT;
                        case 'H': return HOME_KEY;
                        case 'F': return END_KEY;
                        default: break;
                    }
                } else if (seq[1] == '2' && seq[2] == '0') {
                    if (read(STDIN_FILENO, &seq[3], 2) != 2) {
                        return '\x1b';
                    }

                    if (seq[3] == '0' && seq[4] == '~') {
                        return PASTE_INIT;
                    }
                    
                    if (seq[3] == '1' && seq[4] == '~') {
                        return PASTE_END;
                    }
                }
            } else {
                switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN; 
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    default: break;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                default: break;
            }
        } 
        return '\x1b';
    }
    
    return c;
}
