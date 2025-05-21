#include "server.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../error/error.h"
#include "../packet/packet.h"
#include "../net/net.h"

typedef struct pfds {
    struct pollfd* arr;
    size_t fd_count;
    size_t fd_size;
} pfds_t;

#define PORT "9034"
#define BACKLOG 10
#define INIT_PFDS_SIZE 5

int
get_listener_socket(void) {
    int listener;
    int yes = 1;
    int rv;

    struct addrinfo hints, *ai, *p;

    // Gets us a socket and bind it
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "pollserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
            error_log("network err: socket");
            continue;
        }

        // Lose the pesky "addres already in use" error message
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            perror("bind");
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai);

    // We didn't get bound if we got to this point
    if (p == NULL) {
        return -1;
    }

    if (listen(listener, BACKLOG) == -1) {
        return -1;
    }

    printf("server is ready to listen\n");

    return listener;
}

void
add_to_pfds(pfds_t* pfds, int newfd) {
    if (pfds->fd_count == pfds->fd_size) {
        pfds->fd_size *= 2;
        pfds->arr = realloc(pfds->arr, sizeof(struct pollfd) * pfds->fd_size);
    }

    pfds->arr[pfds->fd_count].fd = newfd;
    pfds->arr[pfds->fd_count].events = POLLIN; /* Check ready-to-read */

    ++(pfds->fd_count);
}

void
del_from_pfds(pfds_t* pfds, int i) {
    pfds->arr[i] = pfds->arr[--(pfds->fd_count)];
}

pfds_t*
init_pfds(void) {
    pfds_t* pfds = malloc(sizeof(pfds_t));

    *pfds = (pfds_t) {
        .arr = malloc(sizeof(struct pollfd) * INIT_PFDS_SIZE),
        .fd_count = 0,
        .fd_size = 5
    };

    return pfds;
}

void
free_pfds(pfds_t* pfds) {
    free(pfds->arr);
    free(pfds);
}

int
server(void) {
    int newfd;                          /* Newly accept()ed socket descriptor */
    struct sockaddr_storage remoteaddr; /* Client address */
    socklen_t addrlen;

    char buf[PACKET_SIZE_MAX];          /* Buffer for client data */ 
    char remoteIP[INET6_ADDRSTRLEN];

    pfds_t* pfds = init_pfds();
    int listener = get_listener_socket();

    if (listener == -1) {
        fprintf(stderr, "error getting listening socket\n");
        exit(1);
    }

    // Add the listener to set
    add_to_pfds(pfds, listener);

    while (1) {
        int poll_count = poll(pfds->arr, pfds->fd_count, -1);

        if (poll_count == -1) {
            perror("poll");
            exit(1);
        }

        for (int i = 0; poll_count > 0; ++i) { // keep going until no more events left 
            if (!(pfds->arr[i].revents & POLLIN)) {
                continue;
            }

            --poll_count; // decrease in event

            if (pfds->arr[i].fd == listener) {
                // If listener is ready to read, handle new connection
                addrlen = sizeof(struct sockaddr_storage);

                newfd = accept(listener, (struct sockaddr*)&remoteaddr, &addrlen);

                if (newfd == -1) {
                    perror("accept");
                } else {
                    add_to_pfds(pfds, newfd);

                    printf("pollserver: new connection from %s on socket %d\n",
                            inet_ntop(remoteaddr.ss_family, get_in_addr((struct sockaddr*)&remoteaddr), remoteIP, sizeof(remoteIP)), newfd);
                }
            } else {
                // If not the listener, we're just a regular client
                int nbytes = recv(pfds->arr[i].fd, buf, sizeof(buf), 0);
                int sender_fd = pfds->arr[i].fd;

                if (nbytes <= 0) {
                    if (nbytes == 0) {
                        printf("pollserver: socket %d hung up\n", sender_fd);
                    } else {
                        perror("recv");
                    }

                    close(pfds->arr[i].fd);
                    del_from_pfds(pfds, i);
                    --i; // so you don't skip the deleted one
                } else {
                    for (size_t j = 0; j < pfds->fd_count; ++j) {
                        int dest_fd = pfds->arr[j].fd;

                        if (dest_fd != listener && dest_fd != sender_fd) {
                            if (send(dest_fd, buf, nbytes, 0) == -1) {
                                perror("send");
                            }
                        }
                    }
                }
            }
        }
    }

    free_pfds(pfds);

    return 0;
}
