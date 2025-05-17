
#if !defined(PACKET_H)
#define PACKET_H

#include <stdint.h>
#include <time.h>

#include "../net/net.h"

typedef enum {
    SIZE_MAGIC      = 6,
    SIZE_USRNAME    = 12,
    SIZE_PAYLD_LEN  = 1,
    SIZE_FLAGS      = 1,
    SIZE_CHECKSUM   = 4,
    SIZE_OPTIONS    = 12,
    SIZE_NONCE      = 8,
    SIZE_TIMESTAMP  = 8,
    SIZE_PAYLD      = 255,
} packet_field_size_t;

typedef enum {
    PACKET_SIZE_MIN = 48,
    PACKET_SIZE_MAX = 303,
} packet_size_t;

typedef struct packet {
    uint8_t payld_len;
    uint64_t nonce;
    uint64_t timestamp;
    uint8_t flags;
    char usrname[SIZE_USRNAME + 1];
    char payld[SIZE_PAYLD + 1];
} packet_t;


/*** make ***/

extern packet_t
packet_build(const char* usrname);


/*** send/recv ***/

extern int
packet_recv(packet_t* packet, net_t* net);

extern int
packet_send(packet_t* packet, net_t* net);

#endif /* !defined(PACKET_H) */
