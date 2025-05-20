
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "error/error.h"
#include "server/server.h"
#include "client/client.h"

typedef enum role {
    ROLE_HOST,
    ROLE_JOIN,
} rooms_role_t;

#define MODE_CLIENT "join"
#define MODE_SERVER "host"

#define ARGC_CLIENT(argc) ((argc) == 4 || (argc) == 5)
#define ARGC_SERVER(argc) ((argc) == 2)

rooms_role_t
get_role(int argc, const char** argv) {
    if (ARGC_CLIENT(argc) && strcmp(argv[1], MODE_CLIENT) == 0) {
        error_init(argv[2], MODE_CLIENT);
        return ROLE_JOIN;
    }

    if (ARGC_SERVER(argc) && strcmp(argv[1], MODE_SERVER) == 0) {
        error_init(NULL, MODE_SERVER);
        return ROLE_HOST;
    }

    fprintf(stderr, "usage:\nrooms host\nrooms join <usrname> <ip>\n");

    exit(EXIT_FAILURE);
}


int
main(int argc, const char** argv) {
    return get_role(argc, argv) == ROLE_HOST ? server() : client(argv);
}
