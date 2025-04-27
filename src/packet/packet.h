
#if !defined(PACKET_H)
#define PACKET_H

#include <stdint.h>
#include <time.h>

#include "../net/net.h"

#define MAX_PACKET_SIZE 255
#define MIN_PACKET_SIZE 26

#define LEN_SIZE 1
#define TIME_SIZE 8
#define USRNAME_SIZE 16
#define MSG_SIZE 230

typedef struct packet {
    uint8_t len;
    time_t time;
    char usrname[USRNAME_SIZE + 1];
    char msg[MSG_SIZE + 1];
} packet_t;


/*** encoding ***/

extern void
packet_encode(const packet_t* packet, uint8_t* net_packet);

extern void
packet_decode(const uint8_t* net_packet, packet_t* packet);


/*** make ***/

extern void
packet_fill_usrname(packet_t* packet, const char* usrname);

extern void
packet_seal(packet_t* packet);


/*** send/recv ***/

extern int
packet_recvall(net_t* net, uint8_t* packet_buf);

extern int
packet_sendall(net_t* net, const uint8_t* packet_buf);

#endif /* !defined(PACKET_H) */
