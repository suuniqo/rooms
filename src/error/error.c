
#include "error.h"

#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>


/*** private ***/

#define LOG_FILE "rooms.log"
#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"

#define TIME_BUF_LEN 64
#define ERR_BUF_LEN 64

static size_t
get_err_time(char* tm_buf) {
    time_t epoch = time(NULL);
    struct tm tm_info;

    (void)localtime_r(&epoch, &tm_info);

    return strftime(tm_buf, TIME_BUF_LEN, TIME_FORMAT, &tm_info);
}


/*** methods ***/

void
error_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char tm_buf[TIME_BUF_LEN];
    char err_buf[ERR_BUF_LEN];

    (void)get_err_time(tm_buf);
    if (strerror_r(errno, err_buf, sizeof(err_buf)) != 0) {
        *err_buf = '\0';
    }

    FILE* log = fopen(LOG_FILE, "a");

    if (log == NULL) {
        perror("log err: failed to open rooms_log.txt");
        va_end(args);

        return;
    }

    fprintf(log, "[ERROR][%s] ", tm_buf);
    vfprintf(log, fmt, args);

    if (errno != 0) {
        fprintf(log, ": %s (%d)", err_buf, errno);
        errno = 0;
    }

    fprintf(log, "\n");

    fclose(log);
    va_end(args);
}

void
error_shutdown(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char err_buf[ERR_BUF_LEN];

    vsnprintf(err_buf, sizeof(err_buf), fmt, args);
    va_end(args);

    error_log("%s", err_buf);

    exit(1);
}
