
#include "pfds.h"

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/resource.h>
#include <unistd.h>

#include "../../../log/log.h"

#define RESERVED_FDS 1

/*** methods ***/

pfds_t*
pfds_init(nfds_t size) {
    pfds_t* pfds = malloc(sizeof(pfds_t));

    if (pfds == NULL) {
        log_shutdown("pfds err: malloc");
    }

    *pfds = (pfds_t) {
        .count = 0,
        .size = size,
    };

    pfds->arr = malloc(sizeof(struct pollfd) * size);

    if (pfds->arr == NULL) {
        log_shutdown("pfds err: malloc");
    }
    
    return pfds;
}

void
pfds_free(pfds_t* pfds) {
    free(pfds->arr);
    free(pfds);
}

int
pfds_join(pfds_t* pfds, int newfd, short events) {
    if (pfds->count == pfds->size) {
        if (pfds->size == RLIMIT_NOFILE) {
            return -1;
        }
        
        if (pfds->size > UINT_MAX / 2) {
            pfds->size = UINT_MAX;
        } else {
            pfds->size *= 2;
        }

        struct pollfd* new_arr = realloc(pfds->arr, sizeof(struct pollfd) * pfds->size);

        if (pfds->arr == NULL) {
            log_error("pfds err: realloc");
            return -1;
        }

        pfds->arr = new_arr;
    }

    struct rlimit rl;
    getrlimit(RLIMIT_NOFILE, &rl);
    
    if (pfds->count + 1 >= rl.rlim_cur - RESERVED_FDS) {
        log_error("pfds err: server is full at %d clients", pfds->count - 1);
        return -1;
    }

    pfds->arr[pfds->count++] = (struct pollfd) {
        .fd     = newfd,
        .events = events,
    };

    return 0;
}

void
pfds_exit(pfds_t* pfds, unsigned idx) {
    pfds->arr[idx] = pfds->arr[--pfds->count];
}

int
pfds_poll(pfds_t* pfds) {
    return poll(pfds->arr, pfds->count, -1);
}
