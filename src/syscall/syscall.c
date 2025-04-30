
#include "syscall.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "../error/error.h"

#define MS_IN_SC 1000

/*** sleep ***/

void
safe_nanosleep(long ns) {
    struct timespec req = {ns / ONE_SC, ns % ONE_SC};
    struct timespec rem;

    int saved_errno = errno;

    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            req = rem;
        } else {
            error_shutdown("syscall err: nanosleep");
        }
    }

    errno = saved_errno;
}

int64_t
safe_rand(void) {
    int fd = open("/dev/urandom", O_RDONLY);

    if (fd < 0) {
        error_shutdown("syscall err: open");
    }

    int64_t rand;
    ssize_t bytes = read(fd, &rand, sizeof(uint64_t));

    if (bytes != sizeof(int64_t)) {
        error_shutdown("syscall err: read");
    }

    close(fd);

    return rand;
}

int64_t
safe_time_ms(void) {
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        error_shutdown("syscall err: clock_gettime");
    }

    return (ts.tv_sec * MS_IN_SC) + (ts.tv_nsec / ONE_MS);
}
