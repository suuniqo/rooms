
#if !defined(CCONF_T)
#define CCONF_T

/*** data ***/

typedef struct config {
    const char* usrname;
    const char* ip;
    const char* port;
} cconf_t;

/*** methods ***/

extern void
cconf_init(cconf_t** conf, const char** args);

extern void
cconf_free(cconf_t* conf);

#endif /* !defined(CCONF_T) */
