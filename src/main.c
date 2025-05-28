
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "log/log.h"

#include "cleaner/cleaner.h"
#include "server/server.h"
#include "client/client.h"

typedef enum role {
    ROLE_SERVER,
    ROLE_CLIENT,
} rooms_role_t;

#define MODE_CLIENT "join"
#define MODE_SERVER "host"

#define ARGC_CLIENT(argc) ((argc) == 4 || (argc) == 5)
#define ARGC_SERVER(argc) ((argc) == 2 || (argc) == 3)

rooms_role_t
get_role(int argc, const char** argv) {
    if (ARGC_CLIENT(argc) && strcmp(argv[1], MODE_CLIENT) == 0) {
        return ROLE_CLIENT;
    }

    if (ARGC_SERVER(argc) && strcmp(argv[1], MODE_SERVER) == 0) {
        return ROLE_SERVER;
    }

    fprintf(stderr, "usage:\nrooms host\nrooms join <usrname> <ip>\n");

    exit(EXIT_FAILURE);
}


int
main(int argc, const char** argv) {
    rooms_role_t role = get_role(argc, argv);

    cleaner_init();
    log_init(argv[2], argv[1]);

    return role == ROLE_SERVER ? server(argv + 2) : client(argv + 2);
}
