
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "server/server.h"
#include "client/client.h"

typedef enum role {
    ROLE_HOST,
    ROLE_JOIN,
} rooms_role_t;

#define ARGC_CLIENT(argc) ((argc) == 4 || (argc) == 5)
#define ARGC_SERVER(argc) ((argc) == 2)

rooms_role_t
get_role(int argc, char** argv) {
    if (ARGC_CLIENT(argc) && strcmp(argv[1], "join") == 0) {
        return ROLE_JOIN;
    }

    if (ARGC_SERVER(argc) && strcmp(argv[1], "host") == 0) {
        return ROLE_HOST;
    }

    fprintf(stderr, "usage:\nrooms host\nrooms join <usrname> <ip>\n");
    exit(1);
}

int
main(int argc, char** argv) {
    return get_role(argc, argv) == ROLE_HOST ? server() : client(argv);
}
