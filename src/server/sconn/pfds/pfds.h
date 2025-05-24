
#include <stddef.h>
#include <poll.h>

#if !defined(PFDS_H)
#define PFDS_H

typedef struct pfds {
    struct pollfd* arr;
    nfds_t count;
    nfds_t size;
} pfds_t;

extern pfds_t*
pfds_init(nfds_t size);

extern void
pfds_free(pfds_t* pfds);

extern int
pfds_join(pfds_t* pfds, int newfd, short events);

extern void
pfds_exit(pfds_t* pfds, unsigned idx);

extern int
pfds_poll(pfds_t* pfds);

#endif // !defined(PFDS_H)
