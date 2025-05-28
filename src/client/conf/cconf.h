
#if !defined(CCONF_H)
#define CCONF_H

/*** data ***/

typedef struct config {
    const char* usrname;
    const char* ip;
    const char* port;
} cconf_t;

/*** methods ***/

extern void
cconf_init(cconf_t** conf_ptr, const char** args);

#endif /* !defined(CCONF_H) */
