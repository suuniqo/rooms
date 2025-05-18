
#if !defined(NET_H)
#define NET_H 

/*** includes ***/

#include <netdb.h>
#include <stdatomic.h>

#include "../config/config.h"

/*** data ***/

typedef struct net {
    int sockfd;
} net_t;


/*** utils ***/

extern int
sendall(const net_t* net, const uint8_t* buf, unsigned len);

extern int
recvall(const net_t* net, uint8_t* buf, unsigned len);

extern int
validate_ip(const char* ip);

extern int
validate_port(const char* port);

extern void*
get_in_addr(struct sockaddr* sa);


/*** methods ***/

extern void
net_init(net_t** net, const config_t* config);

extern void
net_reconnect(net_t* net, const config_t* config, atomic_bool* retry, pthread_cond_t* cond, pthread_mutex_t* mutex);

extern void
net_shutdown(net_t* net, int flag);

extern void
net_free(net_t* net);


#endif /* !defined(NET_H) */
