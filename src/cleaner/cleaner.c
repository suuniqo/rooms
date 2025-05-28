
#include "cleaner.h"

#include <stdlib.h>

#include "../log/log.h"

#define MAX_CLEANER_ENTRIES 32

typedef struct {
    cleaner_fn_t fn;
    void** arg;
} cleaner_entry_t;

struct cleaner {
    cleaner_entry_t entries[MAX_CLEANER_ENTRIES];
    unsigned size;
};

static cleaner_t* cleaner = NULL; /* NOLINT */

/*** run ***/

static void
cleaner_free(void) {
    if (cleaner == NULL) {
        return;
    }

    free(cleaner);
}

/*** methods ***/

void
cleaner_run(void) {
    unsigned size = cleaner->size;
    cleaner->size = 0;

    for (unsigned i = size; i-- > 0;) {
        cleaner_entry_t entry = cleaner->entries[i];

        if (entry.arg != NULL) {
            entry.fn(*entry.arg);
        } else {
           ((void (*)(void)) entry.fn)();
        }
    }
}

void
cleaner_init(void) {
    if (cleaner != NULL) {
        return;
    }

    cleaner = malloc(sizeof(cleaner_t));

    if (cleaner == NULL) {
        log_shutdown("cleaner err: malloc");
    }

    cleaner->size = 0;

    cleaner_push((cleaner_fn_t)cleaner_free, NULL);
    atexit(cleaner_run);
}

void
cleaner_push(cleaner_fn_t cleaner_fn, void** arg) {
    if (cleaner->size >= MAX_CLEANER_ENTRIES) {
        log_error("cleaner err: cannot push, cleaner is full");
        return;
    }

    cleaner->entries[cleaner->size++] = (cleaner_entry_t) {
        .arg = arg,
        .fn = cleaner_fn,
    };
}
