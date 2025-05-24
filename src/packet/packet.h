
#if !defined(PACKET_H)
#define PACKET_H

#include <stdint.h>
#include <time.h>

#include "../net/net.h"

#define RESYNC_NOLIMIT -1

typedef enum {
    RECV_DISCONN =  0,
    RECV_ERROR   = -1,
    RECV_INVAL   = -2,
    RECV_DESYNC  = -3,
} packet_recv_err_t;

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
    OFFSET_USRNAME   = OFFSET_MAGIC     + SIZE_MAGIC,
    OFFSET_PAYLD_LEN = OFFSET_USRNAME   + SIZE_USRNAME,
    OFFSET_FLAGS     = OFFSET_PAYLD_LEN + SIZE_PAYLD_LEN,
    OFFSET_CRC       = OFFSET_FLAGS     + SIZE_FLAGS,
    OFFSET_OPTIONS   = OFFSET_CRC       + SIZE_CRC,
    OFFSET_NONCE     = OFFSET_OPTIONS   + SIZE_OPTIONS,
    OFFSET_TIMESTAMP = OFFSET_NONCE     + SIZE_NONCE,
    OFFSET_PAYLD     = OFFSET_TIMESTAMP + SIZE_TIMESTAMP,
} packet_field_offset_t;

typedef enum {
    PACKET_SIZE_MIN = OFFSET_PAYLD,
    PACKET_SIZE_MAX = (int)PACKET_SIZE_MIN + (int)SIZE_PAYLD,
} packet_size_t;

typedef enum {
    PACKET_FLAG_MSG  = 1 << 0,
    PACKET_FLAG_ACK  = 1 << 1,
    PACKET_FLAG_WHSP = 1 << 2,
    PACKET_FLAG_JOIN = 1 << 3,
    PACKET_FLAG_EXIT = 1 << 4,
    PACKET_FLAG_DISC = 1 << 5,
    PACKET_FLAG_PING = 1 << 6,
    PACKET_FLAG_PONG = 1 << 7,
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

extern void
packet_seal(packet_t* packet, uint8_t flags);

/*** send/recv ***/

extern int
packet_recv(packet_t* packet, int sockfd, int max_resyncs);

extern int
packet_send(const packet_t* packet, int sockfd);

/*** ping ***/

extern packet_t
packet_build_ping(const char* usrname);

extern void
packet_build_pong(packet_t* ping);

/*** ack ***/

extern void
packet_build_ack(packet_t* packet);

#endif /* !defined(PACKET_H) */
