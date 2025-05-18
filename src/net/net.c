
#include "net.h"

#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#include "../error/error.h"
#include "../syscall/syscall.h"


/*** defines ***/

#define PORT_MIN 1024
#define PORT_MAX 49151

#define BASE_TEN 10


/*** aux ***/

int
sendall(const net_t* net, const uint8_t* buf, unsigned len) {
    ssize_t n = 0;
    ssize_t sent = 0;

    do {
        n = send(net->sockfd, buf + sent, (size_t)(len - sent), MSG_NOSIGNAL);

        if (n == -1 && errno == EINTR) {
            error_log("net err: send");
            continue;
        }

        if (n <= 0) {
            return (int)n;
        }

        sent += n;

    } while (sent < len);

    return (int)sent;
}

int
recvall(const net_t* net, uint8_t* buf, unsigned len) {
    ssize_t n = 0;
    ssize_t recvd = 0;

     do {
        n = recv(net->sockfd, buf + recvd, (size_t)(len - recvd), MSG_WAITALL);

        if (n == -1 && errno == EINTR) {
            error_log("net err: recv");
            continue;
        }

        if (n <= 0) {
            return (int)n;
        }

        recvd += n;

    } while (recvd < len);

    return (int)recvd;
}



static int
get_socket(const char* ip, const char* port) {
    struct addrinfo hints, *ai, *p;

    int rv = 0;
    int sockfd = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;

    if ((rv = getaddrinfo(ip, port, &hints, &ai)) != 0) {
        error_shutdown("net err: %s\n", gai_strerror(rv));
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            error_log("net err: couldn't get socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        error_log("net err: couldn't connect");

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(ai);

    return sockfd;
}


/*** methods ***/

int
validate_ip(const char* ip) {
    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;

    if (inet_pton(AF_INET, ip, &(sa4.sin_addr)) == 1) {
        return 0;
    }
    if (inet_pton(AF_INET6, ip, &(sa6.sin6_addr)) == 1) {
        return 0;
    }

    return -1;
}

int
validate_port(const char* port) {
    char* endptr = NULL;
    long port_num = strtol(port, &endptr, BASE_TEN);

    if (endptr == port || *endptr != '\0') {
        return -1;
    }

    if (port_num < PORT_MIN || port_num > PORT_MAX) {
        return -1;
    }

    return 0;
}

void*
get_in_addr(struct sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

#define MIN_BACKOFF (ONE_SC / 16)
#define MAX_BACKOFF (ONE_SC * 16)

#define INM_RETRIES 16
#define JITTER_RATIO 8

void
net_reconnect(net_t* net, const config_t* config, atomic_bool* retry, pthread_cond_t* cond, pthread_mutex_t* mutex) {
    if (!atomic_load(retry)) {
        return;
    }

    int64_t backoff, jitter;
    int sockfd;

    pthread_mutex_lock(mutex);

    close(net->sockfd);
    net->sockfd = -1;

    for (unsigned i = 0; i < INM_RETRIES; ++i) {
        sockfd = get_socket(config->ip, config->port);

        if (sockfd != -1) {
            net->sockfd = sockfd;
            pthread_mutex_unlock(mutex);

            return;
        }
    }

    backoff = MIN_BACKOFF;

    while (atomic_load(retry)) {
        sockfd = get_socket(config->ip, config->port);

        if (sockfd != -1) {
            net->sockfd = sockfd;
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
net_init(net_t** net, const config_t* config) {
    *net = malloc(sizeof(net_t));

    if (*net == NULL) {
        error_shutdown("net err: malloc");
    }

    (*net)->sockfd = get_socket(config->ip, config->port);

    if ((*net)->sockfd == -1) {
        error_shutdown("net err: failed to connect");
    }

#if defined(__APPLE__) || defined(__MACH__) /* on macOS SIGPIPE has to be disabled manually */
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        error_shutdown("net err: sigaction");
    }
#endif /* defined(__APPLE__) || defined(__MACH__) */
}

void
net_shutdown(net_t* net, int flag) {
    if (net->sockfd != -1) {
        shutdown(net->sockfd, flag);
    }
}

void
net_free(net_t* net) {
    if (net->sockfd != -1) {
        close(net->sockfd);
    }
    free(net);
}
