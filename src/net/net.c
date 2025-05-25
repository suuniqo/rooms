
#include "net.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "../error/error.h"


/*** defines ***/

#define PORT_MIN 1024
#define PORT_MAX 49151

#define BASE_TEN 10


/*** ioall ***/

int
sendall(int sockfd, const uint8_t* buf, unsigned len) {
    ssize_t n = 0;
    ssize_t sent = 0;

    do {
        n = send(sockfd, buf + sent, (size_t)(len - sent), MSG_NOSIGNAL);

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
recvall(int sockfd, uint8_t* buf, unsigned len) {
    ssize_t recvd = 0;

     do {
        ssize_t n = recv(sockfd, buf + recvd, (size_t)(len - recvd), MSG_WAITALL);

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


/*** connection ***/

int
get_socket_connect(const char* ip, const char* port) {
    struct addrinfo hints, *ai, *p;
    int rv, sockfd = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;

    if ((rv = getaddrinfo(ip, port, &hints, &ai)) != 0) {
        error_shutdown("net err: getaddrinfo: %s\n", gai_strerror(rv));
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            error_log("net err: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        error_log("net err: connect");
        close(sockfd);

        sockfd = -1;
    }

    freeaddrinfo(ai);

    return sockfd;
}

int
get_socket_listen(const char* port) {
    struct addrinfo hints, *ai, *p;
    int rv, sockfd;

    int yes = 1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        error_shutdown("net err: getaddrinfo: %s\n", gai_strerror(rv));
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            error_log("net err: socket");
            continue;
        }

        /* loose "addres already in use" error message */
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            error_shutdown("net err: setsockopt");
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        error_log("net err: bind");
        close(sockfd);

        sockfd = -1;
    }

    freeaddrinfo(ai);

    if (p == NULL) {
        return -1;
    }

    if (listen(sockfd, SOMAXCONN) == -1) {
        error_log("net err: listen");
        close(sockfd);

        return -1;
    }

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
