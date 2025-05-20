
#if !defined(CCONN_H)
#define CCONN_H 

/*** includes ***/

#include <netdb.h>
#include <stdatomic.h>

#include "../conf/cconf.h"

/*** data ***/

typedef struct cconn {
    int sockfd;
} cconn_t;


/*** methods ***/

extern void
cconn_init(cconn_t** conn, const cconf_t* config);

extern void
cconn_reconnect(cconn_t* conn, const cconf_t* config, atomic_bool* retry, pthread_cond_t* cond, pthread_mutex_t* mutex);

extern int
cconn_get_socket(cconn_t* conn);

extern void
cconn_shutdown(cconn_t* conn, int flag);

extern void
cconn_free(cconn_t* conn);


#endif /* !defined(CCONN_H) */
