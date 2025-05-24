
#include "cleaner.h"

#include <stdlib.h>

#include "../../../error/error.h"

#define MAX_CLEANER_ENTRIES 7

typedef struct {
    cleaner_fn_t fn;
    void* arg;
} cleaner_entry_t;

struct cleaner {
    cleaner_entry_t entries[MAX_CLEANER_ENTRIES];
    unsigned size;
};

void
cleaner_init(cleaner_t** cleaner) {
    *cleaner = malloc(sizeof(cleaner_t));

    if (*cleaner == NULL) {
        error_shutdown("cleaner err: malloc");
    }

    (*cleaner)->size = 0;
}

void
cleaner_free(cleaner_t* cleaner) {
    free(cleaner);
}

void
cleaner_push(cleaner_t* cleaner, cleaner_fn_t cleaner_fn, void* arg) {
    if (cleaner->size >= MAX_CLEANER_ENTRIES) {
        error_log("cleaner err: cannot push, cleaner is full");
        return;
    }

    cleaner->entries[cleaner->size++] = (cleaner_entry_t) {
        .arg = arg,
        .fn = cleaner_fn,
    };
}

void
cleaner_run(cleaner_t* cleaner) {
    unsigned size = cleaner->size;
    cleaner->size = 0;

    for (unsigned i = size; i-- > 0;) {
        cleaner_entry_t entry = cleaner->entries[i];

        if (entry.arg != NULL) {
            entry.fn(entry.arg);
        } else {
           ((void (*)(void)) entry.fn)();
        }
    }
}
