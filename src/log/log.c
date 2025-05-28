
#include "log.h"

#include <errno.h>
#include <pthread.h>
#include <string.h>
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
    LOG_NORMAL,
    LOG_ERROR,
    LOG_FATAL,
    LOG_LEN,
} log_type_t;

typedef struct log {
    pthread_mutex_t lock;
    char file[LOG_NAME_LEN];
} log_t;

static log_t* log = NULL; /* NOLINT */

static const char* const error_type_str[LOG_LEN] = {"LOGGN", "ERROR", "FATAL"};

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
log_free(void) {
    if (log == NULL) {
        return;
    }

    pthread_mutex_destroy(&log->lock);
    free(log);
}

void
log_init(const char* user, const char* mode) {
    if (log != NULL) {
        return;
    }

    log = malloc(sizeof(log_t));

    if (log == NULL) {
        perror("log err: malloc");
        exit(EXIT_FAILURE);
    }

    if (user != NULL) {
        (void)snprintf(log->file, LOG_NAME_LEN, "logs/rooms-%s@%s.log", mode, user);
    } else {
        (void)snprintf(log->file, LOG_NAME_LEN, "logs/rooms-%s.log", mode);
    }

    pthread_mutex_init(&log->lock, NULL);

    atexit(log_free);
}

void
log_write(const char* errmsg, log_type_t type) {
    pthread_mutex_lock(&log->lock);

    int saved_errno = errno;

    char tm_buf[TIME_BUF_LEN];
    char err_buf[ERR_BUF_LEN];

    if (error_ftime(tm_buf) == 0) {
        *tm_buf = '\0';
    }
    if (strerror_r(errno, err_buf, sizeof(err_buf)) != 0) {
        *err_buf = '\0';
    }

    FILE* logfd = fopen(log->file, "a");

    if (log == NULL) {
        perror("log err: failed to open rooms_log.txt");
        exit(EXIT_FAILURE);
    }

    fprintf(logfd, "[%s][%s] %s", error_type_str[type], tm_buf, errmsg);

    if (errno != 0) {
        fprintf(logfd, ": %s (%d)", err_buf, saved_errno);
        errno = 0;
    }

    fprintf(logfd, "\n");
    fclose(logfd);

    pthread_mutex_unlock(&log->lock);
}

void
log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char err_buf[ERR_BUF_LEN];

    vsnprintf(err_buf, sizeof(err_buf), fmt, args);
    va_end(args);

    log_write(err_buf, LOG_NORMAL);
}

void
log_shutdown(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    char err_buf[ERR_BUF_LEN];

    vsnprintf(err_buf, sizeof(err_buf), fmt, args);
    va_end(args);

    log_write(err_buf, LOG_FATAL);

    exit(EXIT_FAILURE);
}
