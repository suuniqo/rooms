
#include "error.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>


/*** data ***/

#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"

#define TIME_BUF_LEN 64
#define ERR_BUF_LEN 64

#define LOG_NAME_LEN 32

typedef enum {
    ERROR_TYPE_NORMAL,
    ERROR_TYPE_FATAL,
    ERROR_TYPE_LEN,
} error_type_t;

static char log_file[LOG_NAME_LEN] = {0}; /* NOLINT */
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER; /* NOLINT */
static const char* const error_type_str[ERROR_TYPE_LEN] = {"ERROR", "FATAL"};


/*** aux ***/

static size_t
error_ftime(char* tm_buf) {
    time_t epoch = time(NULL);
    struct tm tm_info;

    if (localtime_r(&epoch, &tm_info) == NULL) {
        perror("log err: couldn't get time");
        return 0;
    }

    return strftime(tm_buf, TIME_BUF_LEN, TIME_FORMAT, &tm_info);
}


/*** methods ***/

void
error_free(void) {
    pthread_mutex_destroy(&log_lock);
}

void
error_init(const char* user, const char* mode) {
    if (user != NULL) {
        (void)snprintf((char*)log_file, LOG_NAME_LEN, "logs/rooms-%s@%s.log", mode, user);
    } else {
        (void)snprintf((char*)log_file, LOG_NAME_LEN, "logs/rooms-%s.log", mode);
    }

    atexit(error_free);
}

void
error_write(const char* errmsg, error_type_t type) {
    pthread_mutex_lock(&log_lock);

    int saved_errno = errno;

    char tm_buf[TIME_BUF_LEN];
    char err_buf[ERR_BUF_LEN];

    if (error_ftime(tm_buf) == 0) {
        *tm_buf = '\0';
    }
    if (strerror_r(errno, err_buf, sizeof(err_buf)) != 0) {
        *err_buf = '\0';
    }

    FILE* log = fopen(log_file, "a");

    if (log == NULL) {
        perror("log err: failed to open rooms_log.txt");
        exit(1);
    }

    fprintf(log, "[%s][%s] %s", error_type_str[type], tm_buf, errmsg);

    if (errno != 0) {
        fprintf(log, ": %s (%d)", err_buf, saved_errno);
        errno = 0;
    }

    fprintf(log, "\n");
    fclose(log);

    pthread_mutex_unlock(&log_lock);
}

void
error_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char err_buf[ERR_BUF_LEN];

    vsnprintf(err_buf, sizeof(err_buf), fmt, args);
    va_end(args);

    error_write(err_buf, ERROR_TYPE_NORMAL);
}

void
error_shutdown(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char err_buf[ERR_BUF_LEN];

    vsnprintf(err_buf, sizeof(err_buf), fmt, args);
    va_end(args);

    error_write(err_buf, ERROR_TYPE_FATAL);

    exit(1);
}
