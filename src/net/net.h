
#if !defined(NET_H)
#define NET_H 

/*** includes ***/

#include <netdb.h>

#include "../config/config.h"

/*** data ***/

typedef struct net {
    int sockfd;
} net_t;

/*** methods ***/

extern int
validate_ip(const char* ip);

extern int
validate_port(const char* port);

extern void*
get_in_addr(struct sockaddr* sa);

extern void
net_init(net_t** net, const config_t* config);

extern void
net_shutdown(net_t* net, int flag);

extern void
net_free(net_t* net);

#endif /* !defined(NET_H) */
