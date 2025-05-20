
#if !defined(PACKET_H)
#define PACKET_H

#include <stdint.h>
#include <time.h>

#include "../net/net.h"

typedef enum {
    SIZE_MAGIC     = 6,
    SIZE_USRNAME   = 10,
    SIZE_PAYLD_LEN = 1,
    SIZE_FLAGS     = 1,
    SIZE_CRC       = 4,
    SIZE_OPTIONS   = 10,
    SIZE_NONCE     = 8,
    SIZE_TIMESTAMP = 8,
    SIZE_PAYLD     = 255,
} packet_field_size_t;

typedef enum {
    OFFSET_MAGIC     = 0,
    OFFSET_USRNAME   = 6,
    OFFSET_PAYLD_LEN = 16,
    OFFSET_FLAGS     = 17,
    OFFSET_CRC       = 18,
    OFFSET_OPTIONS   = 22,
    OFFSET_NONCE     = 32,
    OFFSET_TIMESTAMP = 40,
    OFFSET_PAYLD     = 48,
} packet_field_offset_t;

typedef enum {
    PACKET_SIZE_MIN = 48,
    PACKET_SIZE_MAX = 303,
} packet_size_t;

typedef enum {
    PACKET_FLAG_MSG  = 1 << 0,
    PACKET_FLAG_ACK  = 1 << 1,
    PACKET_FLAG_WHSP = 1 << 2,
    PACKET_FLAG_JOIN = 1 << 3,
    PACKET_FLAG_EXIT = 1 << 4,
    PACKET_FLAG_PING = 1 << 5,
    PACKET_FLAG_PONG = 1 << 6,
} packet_flags_t;

typedef struct packet {
    uint8_t payld_len;
    uint8_t flags;
    uint32_t crc;
    uint64_t nonce;
    uint64_t timestamp;
    char usrname[SIZE_USRNAME + 1];
    char options[SIZE_USRNAME + 1];
    char payld[SIZE_PAYLD + 1];
} packet_t;


/*** make ***/

extern packet_t
packet_build(const char* usrname);


/*** send/recv ***/

extern int
packet_recv(packet_t* packet, int sockfd);

extern int
packet_send(packet_t* packet, int sockfd, uint8_t flags);

#endif /* !defined(PACKET_H) */
