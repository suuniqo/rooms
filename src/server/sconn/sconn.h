
#if !defined(SCONN_H)
#define SCONN_H 

typedef struct pfds pfds_t;

typedef struct sconn {
    pfds_t* pfds;
    int listener;
} sconn_t;

extern void
sconn_init(sconn_t** conn, const char* port);

extern void
sconn_free(sconn_t* conn);

extern int
sconn_accept(const sconn_t* conn, int timeout_ms);

extern void
sconn_kick(const sconn_t* conn, unsigned idx);

extern int
sconn_poll(const sconn_t* conn);

extern unsigned
sconn_get_count(const sconn_t* conn);

extern struct pollfd
sconn_get_pfd(const sconn_t* conn, unsigned id);

#endif /* !defined(SCONN_H) */
