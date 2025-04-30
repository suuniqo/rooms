
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


/*** aux ***/

static size_t
get_msg_len(uint8_t packet_len) {
    return (size_t)packet_len - LEN_SIZE - USRNAME_SIZE - TIME_SIZE;
}


/*** serialize ***/

#define U64_HI(VAR) ((VAR) >> 32)
#define U64_LO(VAR) ((VAR) & 0xFFFFFFFF)
#define U64_FROM_HALVES(HIGH, LOW) (((uint64_t)(HIGH) << 32) | (LOW))

static void
pack_time(uint64_t time, uint8_t* buf) {
    uint64_t time64 = time;
    uint32_t high = htonl((uint32_t) U64_HI(time64));
    uint32_t low =  htonl((uint32_t) U64_LO(time64));

    uint32_t* buf32 = (uint32_t*)buf;
    buf32[0] = high;
    buf32[1] = low;
}

static time_t
unpack_time(const uint8_t* buf) {
    const uint32_t* buf32 = (const uint32_t*)buf;
    uint32_t high = ntohl(buf32[0]);
    uint32_t low = ntohl(buf32[1]);

    uint64_t time = U64_FROM_HALVES(high, low);

    if (sizeof(time_t) == sizeof(uint32_t)) {
        return time <= UINT32_MAX ? (time_t)time : 0;
    }

    return (time_t)time;
}


/*** make ***/


void
packet_fill_usrname(packet_t* packet, const char* usrname) {
    strncpy(packet->usrname, usrname, USRNAME_SIZE);
    packet->usrname[USRNAME_SIZE] = '\0';
}

void
packet_seal(packet_t* packet) {
    packet->time = time(NULL);
    packet->len = (uint8_t)strlen(packet->msg) + LEN_SIZE + TIME_SIZE + USRNAME_SIZE;
}


/*** encoding ***/

void
packet_encode(const packet_t* packet, uint8_t* net_packet) {
    size_t usrname_len = strlen(packet->usrname);
    size_t msg_len = get_msg_len(packet->len);

    uint8_t* buf_p = net_packet;
    memset(buf_p, 0, MAX_PACKET_SIZE);

    *buf_p = packet->len;                           buf_p += LEN_SIZE;
    pack_time((uint64_t)packet->time, buf_p);       buf_p += TIME_SIZE;
    memcpy(buf_p, packet->usrname, usrname_len);    buf_p += USRNAME_SIZE;
    memcpy(buf_p, packet->msg, msg_len);
}

void
packet_decode(const uint8_t* net_packet, packet_t* packet) {
    const uint8_t* buf_p = net_packet;

    size_t msg_len = get_msg_len(*buf_p);

    memset(packet->usrname, 0, USRNAME_SIZE + 1);
    memset(packet->msg, 0, msg_len + 1);

    packet->len = *buf_p;                                           buf_p += LEN_SIZE; 
    packet->time = unpack_time(buf_p);                              buf_p += TIME_SIZE;
    strncpy(packet->usrname, (const char*)buf_p, USRNAME_SIZE);     buf_p += USRNAME_SIZE;
    strncpy(packet->msg, (const char*)buf_p, msg_len);
}


/*** send/recv ***/

int
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

int
packet_sendall(net_t* net, const uint8_t* packet_buf) {
    ssize_t n = 0;
    ssize_t sent = 0;

    ssize_t len = (ssize_t)packet_buf[0];

    while (sent < len) {
        n = send(net->sockfd, packet_buf + sent, (size_t)(len - sent), 0);

        if (n < 0) {
            break;
        }

        sent += n;
    }

    return sent == len ? 0 : -1;
}
