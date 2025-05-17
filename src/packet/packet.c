
#include "packet.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../error/error.h"
#include "../syscall/syscall.h"


/*** aux ***/

static size_t
get_msg_len(uint8_t packet_len) {
    return (size_t)packet_len - SIZE_PAYLD_LEN - SIZE_USRNAME - SIZE_TIMESTAMP;
}


/*** serialize ***/

#define U64_HI(VAR) ((VAR) >> 32)
#define U64_LO(VAR) ((VAR) & 0xFFFFFFFF)
#define U64_FROM_HALVES(HIGH, LOW) (((uint64_t)(HIGH) << 32) | (LOW))

static void
pack_u64(uint64_t data, uint8_t* buf) {
    uint64_t data64 = data;
    uint32_t high = htonl((uint32_t) U64_HI(data64));
    uint32_t low =  htonl((uint32_t) U64_LO(data64));

    uint32_t* buf32 = (uint32_t*)buf;
    buf32[0] = high;
    buf32[1] = low;
}

static uint64_t
unpack_u64(const uint8_t* buf) {
    const uint32_t* buf32 = (const uint32_t*)buf;

    uint32_t high = ntohl(buf32[0]);
    uint32_t low = ntohl(buf32[1]);

    uint64_t data = U64_FROM_HALVES(high, low);

    return data;
}

/*** encoding ***/

static void
packet_seal(packet_t* packet) {
    time_t tm = time(NULL);

    if (tm < 0) {
        error_shutdown("packet err: time");
    }

    packet->timestamp = (uint64_t)tm;
    packet->payld_len = (uint8_t)strlen(packet->payld) + SIZE_PAYLD_LEN + SIZE_TIMESTAMP + SIZE_USRNAME;

    packet->nonce += 1;
}

static void
packet_encode(const packet_t* packet, uint8_t* packet_buf) {
    size_t usrname_len = strlen(packet->usrname);
    size_t msg_len = get_msg_len(packet->payld_len);

    memset(packet_buf, 0, PACKET_SIZE_MAX);

    *packet_buf = packet->payld_len;                      packet_buf += SIZE_PAYLD_LEN;
    pack_u64(packet->nonce, packet_buf);                  packet_buf += SIZE_NONCE;
    pack_u64(packet->timestamp, packet_buf);              packet_buf += SIZE_TIMESTAMP;
    *packet_buf = packet->flags;                          packet_buf += SIZE_FLAGS;
    memcpy(packet_buf, packet->usrname, usrname_len);     packet_buf += SIZE_USRNAME;
    memcpy(packet_buf, packet->payld, msg_len);
}

static void
packet_decode(packet_t* packet, const uint8_t* packet_buf) {
    const uint8_t* buf_p = packet_buf;

    size_t msg_len = get_msg_len(*buf_p);

    memset(packet->usrname, 0, SIZE_USRNAME + 1);
    memset(packet->payld, 0, msg_len + 1);

    packet->payld_len = *buf_p;                                           buf_p += SIZE_PAYLD_LEN; 
    packet->timestamp = unpack_u64(buf_p);                          buf_p += SIZE_TIMESTAMP;
    strncpy(packet->usrname, (const char*)buf_p, SIZE_USRNAME);     buf_p += SIZE_USRNAME;
    strncpy(packet->payld, (const char*)buf_p, msg_len);
}


/*** send/recv ***/

static int
packet_recvall(net_t* net, uint8_t* packet_buf) {
    int recvd, bytes;

    bytes = (int)recv(net->sockfd, packet_buf, 1, MSG_WAITALL);
    recvd = bytes;

    if (bytes <= 0) {
        return bytes;
    }

    ssize_t len = (ssize_t)packet_buf[0];

    bytes = (int)recv(net->sockfd, packet_buf + recvd, (size_t)(len - recvd), MSG_WAITALL);
    recvd += bytes;

    if (bytes <= 0) {
        return bytes;
    }

    packet_buf[0] = (uint8_t)recvd;

    return recvd;
}

static int
packet_sendall(net_t* net, const uint8_t* packet_buf) {
    ssize_t n = 0;
    ssize_t sent = 0;

    ssize_t len = (ssize_t)packet_buf[0];

    while (sent < len) {
        n = send(net->sockfd, packet_buf + sent, (size_t)(len - sent), MSG_NOSIGNAL);

        if (n < 0) {
            break;
        }

        sent += n;
    }

    return sent == len ? 0 : -1;
}


/*** methods ***/

packet_t
packet_build(const char* usrname) {
    packet_t packet;

    strncpy(packet.usrname, usrname, SIZE_USRNAME);
    packet.usrname[SIZE_USRNAME] = '\0';

    packet.nonce = (uint64_t)safe_rand();

    return packet;
}

int
packet_send(packet_t* packet, net_t* net) {
    uint8_t packet_buf[PACKET_SIZE_MAX] = {0};

    packet_seal(packet);
    packet_encode(packet, packet_buf);

    return packet_sendall(net, packet_buf);
}

int
packet_recv(packet_t* packet, net_t* net) {
    uint8_t packet_buf[PACKET_SIZE_MAX] = {0};

    int bytes = packet_recvall(net, packet_buf);

    if (bytes > 0) {
        packet_decode(packet, packet_buf);
    }

    return bytes;
}
