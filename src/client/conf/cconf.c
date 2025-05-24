
#include "cconf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../conf.h"

#include "../../error/error.h"
#include "../../net/net.h"
#include "../../packet/packet.h"

/*** data ***/


typedef enum {
    POS_USRNSME = 2,
    POS_IP,
    POS_PORT,
} cargs_t;


/*** aux ***/

static const char*
cconf_extract_usrname(const char** args) {
    const char* usrname = args[POS_USRNSME];

    size_t len = strlen(usrname);

    if (len == 0 || len > SIZE_USRNAME) {
        error_shutdown("cconf err: usrname length must be between 1 and %d", SIZE_USRNAME);
    }

    return usrname;
}

static const char*
cconf_extract_ip(const char** args) {
    const char* ip = args[POS_IP];
    
    if (validate_ip(ip) != 0) {
        error_shutdown("cconf err: invalid ip address");
    }

    return ip;
}

static const char*
cconf_extract_port(const char** args) {
    const char* port = args[POS_PORT];

    if (port == NULL) {
        return DEFAULT_PORT;
    }
    
    if (validate_port(port) != 0) {
        error_shutdown("cconf err: invalid port number");
    }

    return port;
}


/*** methods ***/

void
cconf_init(cconf_t** conf, const char** args) {
    *conf = malloc(sizeof(cconf_t));

    if (*conf == NULL) {
        error_shutdown("cconf err: malloc");
    }

    **conf = (cconf_t) {
        .port    = cconf_extract_port(args),
        .ip      = cconf_extract_ip(args),
        .usrname = cconf_extract_usrname(args)
   };
}


void
cconf_free(cconf_t* conf) {
    free(conf);
}
