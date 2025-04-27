
#if !defined(CONFIG_T)
#define CONFIG_T

/*** data ***/

typedef struct config {
    const char* usrname;
    const char* ip;
    const char* port;
} config_t;

/*** methods ***/

extern void
config_init(config_t** config, char** args);

extern void
config_free(config_t* config);

#endif /* !defined(CONFIG_T) */
