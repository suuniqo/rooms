
#include "cconn.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include "../../error/error.h"
#include "../../syscall/syscall.h"
#include "../../net/net.h"

void
cconn_init(cconn_t** conn, const cconf_t* config) {
    *conn = malloc(sizeof(cconn_t));

    if (*conn == NULL) {
        error_shutdown("conn err: malloc");
    }

    (*conn)->sockfd = get_socket(config->ip, config->port);

    if ((*conn)->sockfd == -1) {
        error_shutdown("conn err: failed to connect");
    }

#if defined(__APPLE__) || defined(__MACH__) /* on macOS SIGPIPE has to be disabled manually */
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        error_shutdown("conn err: sigaction");
    }
#endif /* defined(__APPLE__) || defined(__MACH__) */
}

#define MIN_BACKOFF (ONE_SC / 16)
#define MAX_BACKOFF (ONE_SC * 16)

#define INM_RETRIES 16
#define JITTER_RATIO 8

void
cconn_reconnect(cconn_t* conn, const cconf_t* config, atomic_bool* retry, pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (!atomic_load(retry)) {
        return;
    }

    int64_t backoff, jitter;
    int sockfd;

    pthread_mutex_lock(mutex);

    close(conn->sockfd);
    conn->sockfd = -1;

    for (unsigned i = 0; i < INM_RETRIES; ++i) {
        sockfd = get_socket(config->ip, config->port);

        if (sockfd != -1) {
            conn->sockfd = sockfd;
            pthread_mutex_unlock(mutex);

            return;
        }
    }

    backoff = MIN_BACKOFF;

    while (atomic_load(retry)) {
        sockfd = get_socket(config->ip, config->port);

        if (sockfd != -1) {
            conn->sockfd = sockfd;
            pthread_mutex_unlock(mutex);

            return;
        }

        jitter = safe_rand() % (backoff / JITTER_RATIO);

        if (backoff < MAX_BACKOFF) {
            backoff *= 2;
        }

        struct timespec now, timeout;
        clock_gettime(CLOCK_REALTIME, &now);

        timeout.tv_sec = now.tv_sec + (now.tv_nsec + backoff + jitter) / ONE_SC;
        timeout.tv_nsec = (now.tv_nsec + backoff + jitter) % ONE_SC;

        pthread_cond_timedwait(cond, mutex, &timeout);

    };

    pthread_mutex_unlock(mutex);
}

void
cconn_shutdown(cconn_t* conn, int flag) {
    if (conn->sockfd != -1) {
        shutdown(conn->sockfd, flag);
    }
}

void
cconn_free(cconn_t* conn) {
    if (conn->sockfd != -1) {
        close(conn->sockfd);
    }

    free(conn);
}
