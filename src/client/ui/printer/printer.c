
#include "printer.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "../../../error/error.h"

#define PRINTER_INIT_SIZE 512

struct printer {
    char* buf;
    size_t len;
    size_t size;
};

/*** methods ***/

void
printer_init(printer_t** printer) {
    *printer = malloc(sizeof(printer_t));

    if (*printer == NULL) {
        error_shutdown("printer err: malloc: no mem for printer");
    }

    **printer = (printer_t) {
        .buf = malloc(PRINTER_INIT_SIZE),
        .size = PRINTER_INIT_SIZE,
        .len = 0,
    };

    if ((*printer)->buf == NULL) {
        error_shutdown("printer err: malloc: no mem for printer buf");
    }
}

void
printer_free(printer_t* printer) {
    free(printer->buf);
    free(printer);
}

void
printer_append(printer_t* printer, const char *fmt, ...) {
    va_list args;
    va_list args_cp;

    va_start(args, fmt);
    va_copy(args_cp, args);
    
    size_t len = vsnprintf(NULL, 0, fmt, args_cp);
    va_end(args_cp);

    if (len + printer->len >= printer->size) {
        if (ULONG_MAX - len < printer->size) {
            va_end(args);
            error_shutdown("printer err: printer is full");
        }

        size_t new_size = printer->len + len + 1;

        char* new_buf = realloc(printer->buf, new_size);

        if (new_buf == NULL) {
            va_end(args);
            error_shutdown("printer err: malloc: no mem to resize printer");
        }

        printer->buf = new_buf;
        printer->size = new_size;
    }

    printer->len += vsnprintf(printer->buf + printer->len, printer->size - printer->len, fmt, args);
    va_end(args);
}

void
printer_dump(printer_t* printer) {
    if (write(STDOUT_FILENO, printer->buf, printer->len) == -1) {
        error_shutdown("printer err: couldn't write on screen");
    }

    printer->len = 0;
}
