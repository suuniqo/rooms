
#include "sconf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../conf.h"

#include "../../log/log.h"
#include "../../net/net.h"

/*** data ***/

typedef enum {
    POS_PORT = 0,
} sargs_t;


/*** extract ***/

static const char*
sconf_extract_port(const char** args) {
    const char* port = args[POS_PORT];

    if (port == NULL) {
        return DEFAULT_PORT;
    }
    
    if (validate_port(port) != 0) {
        log_shutdown("sconf err: invalid port number");
    }

    return port;
}


/*** methods ***/

void
sconf_init(sconf_t** conf, const char** args) {
    *conf = malloc(sizeof(sconf_t));

    if (*conf == NULL) {
        log_shutdown("sconf err: malloc");
    }

    **conf = (sconf_t) {
        .port = sconf_extract_port(args),
   };
}


void
sconf_free(sconf_t* conf) {
    free(conf);
}
