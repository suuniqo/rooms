
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../error/error.h"
#include "../net/net.h"
#include "../packet/packet.h"

/*** data ***/

typedef enum {
    POS_USRNSME = 2,
    POS_IP,
    POS_PORT,
} args_t;

#define DEFAULT_PORT "9034"

/*** aux ***/

static char*
config_get_usrname(char** args) {
    char* usrname = args[POS_USRNSME];

    size_t len = strlen(usrname);

    if (len == 0 || len > SIZE_USRNAME) {
        error_shutdown("args err: usrname length must be between 1 and 16");
    }

    for (char* p = usrname; *p != '\0'; ++p) {
        if (*p == ' ') {
            *p = '_';
        }
    }

    return usrname;
}

static char*
config_get_ip(char** args) {
    char* ip = args[POS_IP];
    
    if (validate_ip(ip) != 0) {
        error_shutdown("args err: invalid ip address");
    }

    return ip;
}

static char*
config_get_port(char** args) {
    char* port = args[POS_PORT];

    if (port == NULL) {
        return DEFAULT_PORT;
    }
    
    if (validate_port(port) != 0) {
        error_shutdown("args err: invalid port number");
    }

    return port;
}


/*** methods ***/

void
config_init(config_t** config, char** args) {
    *config = malloc(sizeof(config_t));

    if (*config == NULL) {
        error_shutdown("config err: malloc");
    }

    **config = (config_t) {
        .port = config_get_port(args),
        .ip = config_get_ip(args),
        .usrname = config_get_usrname(args)
   };
}

void
config_free(config_t* config) {
    free(config);
}
