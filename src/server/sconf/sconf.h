
#if !defined(SCONF_H)
#define SCONF_H

/*** data ***/

typedef struct config {
    const char* port;
} sconf_t;

/*** methods ***/

extern void
sconf_init(sconf_t** conf, const char** args);

extern void
sconf_free(sconf_t* conf);

#endif /* !defined(SCONF_H) */
