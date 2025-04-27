
#include "net.h"

#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../error/error.h"


/*** defines ***/

#define PORT_MIN 1024
#define PORT_MAX 49151

#define BASE_TEN 10


/*** aux ***/

static int
get_socket(const char* ip, const char* port) {
    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;

    int rv = 0;
    int sockfd = -1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;

    if ((rv = getaddrinfo(ip, port, &hints, &ai)) != 0) {
        error_shutdown("network err: %s\n", gai_strerror(rv));
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            error_log("network err: couldn't get socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        error_log("network err: couldn't connect");

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(ai);

    if (sockfd < 0) {
        error_shutdown("network err: failed to connect");
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

void
net_init(net_t** net, const config_t* config) {
    *net = malloc(sizeof(net_t));

    if (*net == NULL) {
        error_shutdown("net err: malloc");
    }

    (*net)->sockfd = get_socket(config->ip, config->port);
}

void
net_shutdown(net_t* net, int flag) {
    shutdown(net->sockfd, flag);
}

void
net_free(net_t* net) {
    close(net->sockfd);
    free(net);
}
