
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

#include "../crypto/crc/crc32.h"
#include "../error/error.h"
#include "../syscall/syscall.h"

/*** data ***/

const char* const PACKET_MAGIC = "rooms";


/*** serialize ***/

#define U64_HI(VAR) ((VAR) >> 32)
#define U64_LO(VAR) ((VAR) & 0xFFFFFFFF)
#define U64_FROM_HALVES(HIGH, LOW) (((uint64_t)(HIGH) << 32) | (LOW))

/* pack */
static void
pack_chr(uint8_t* buf, uint8_t byte) {
    *buf = byte;
}

static void
pack_str(uint8_t* buf, const char* data) {
    memcpy(buf, data, strlen(data));
}

static void
pack_u32(uint8_t* buf, uint32_t data) {
    uint32_t* buf32 = (uint32_t*)buf;
    *buf32 = htonl(data);
}

static void
pack_u64(uint8_t* buf, uint64_t data) {
    uint64_t data64 = data;
    uint32_t high = htonl((uint32_t) U64_HI(data64));
    uint32_t low =  htonl((uint32_t) U64_LO(data64));

    uint32_t* buf32 = (uint32_t*)buf;
    buf32[0] = high;
    buf32[1] = low;
}

/* unpack */
static void
unpack_chr(uint8_t* field, const uint8_t* buf) {
    *field = *buf;
}

static void
unpack_str(char* field, const uint8_t* buf, size_t size) {
    memcpy(field, (const char*)buf, size);

    field[size] = '\0';
}

static void
unpack_u32(uint32_t* field, const uint8_t* buf) {
    const uint32_t* buf32 = (const uint32_t*)buf;

    *field = ntohl(*buf32);
}

static void
unpack_u64(uint64_t* field, const uint8_t* buf) {
    const uint32_t* buf32 = (const uint32_t*)buf;

    uint32_t high = ntohl(buf32[0]);
    uint32_t low = ntohl(buf32[1]);

    *field = U64_FROM_HALVES(high, low);
}


/*** encoding ***/

static void
packet_seal(packet_t* packet) {
    time_t tm = time(NULL);

    if (tm < 0) {
        error_shutdown("packet err: time");
    }

    memcpy(packet->magic, PACKET_MAGIC, SIZE_MAGIC);

    packet->timestamp = (uint64_t)tm;
    packet->payld_len = (uint8_t)strlen(packet->payld);
    packet->crc = crc32_generate(packet->payld, packet->payld_len);
    packet->nonce += 1;
}

static void
packet_encode(const packet_t* packet, uint8_t* buf) {
    pack_str(buf, packet->magic);                       buf += SIZE_MAGIC;
    pack_str(buf, packet->usrname);                     buf += SIZE_USRNAME;

    pack_chr(buf, packet->payld_len);                   buf += SIZE_PAYLD_LEN;
    pack_chr(buf, packet->flags);                       buf += SIZE_FLAGS;
    pack_u32(buf, packet->crc);                         buf += SIZE_CRC;
    pack_str(buf, packet->options);                     buf += SIZE_OPTIONS;

    pack_u64(buf, packet->nonce);                       buf += SIZE_NONCE;
    pack_u64(buf, packet->timestamp);                   buf += SIZE_TIMESTAMP;

    pack_str(buf, packet->payld);
}

static void
packet_decode(packet_t* packet, const uint8_t* buf) {
    unpack_str(packet->magic, buf, SIZE_MAGIC);         buf += SIZE_MAGIC;
    unpack_str(packet->usrname, buf, SIZE_USRNAME);     buf += SIZE_USRNAME;

    unpack_chr(&packet->payld_len, buf);                buf += SIZE_PAYLD_LEN;
    unpack_chr(&packet->flags, buf);                    buf += SIZE_FLAGS;
    unpack_u32(&packet->crc, buf);                      buf += SIZE_CRC;
    unpack_str(packet->options, buf, SIZE_OPTIONS);     buf += SIZE_OPTIONS;

    unpack_u64(&packet->nonce, buf);                    buf += SIZE_NONCE;
    unpack_u64(&packet->timestamp, buf);                buf += SIZE_TIMESTAMP;
}


/*** send/recv ***/

#define PAYLD_LEN_OFFSET (SIZE_MAGIC + SIZE_USRNAME)

static int
packet_sendall(const net_t* net, const uint8_t* packet_buf) {
    ssize_t n = 0;
    ssize_t sent = 0;

    ssize_t len = (ssize_t)((uint8_t)PACKET_SIZE_MIN + packet_buf[PAYLD_LEN_OFFSET]);

    while (sent < len) {
        n = send(net->sockfd, packet_buf + sent, (size_t)(len - sent), MSG_NOSIGNAL);

        if (n < 0) {
            break;
        }

        sent += n;
    }

    return sent == len ? 0 : -1;
}

static int
packet_recvhead(const net_t* net, uint8_t* packet_buf) {
    return (int)recv(net->sockfd, packet_buf, PACKET_SIZE_MIN, MSG_WAITALL);
}

static int
packet_recvpayld(const net_t* net, char* payld, uint8_t payld_len) {
    return (int)recv(net->sockfd, payld, payld_len, MSG_WAITALL);
}


/*** methods ***/

packet_t
packet_build(const char* usrname) {
    packet_t packet = {0};

    memcpy(packet.usrname, usrname, strlen(usrname));

    packet.nonce = (uint64_t)safe_rand();

    return packet;
}

int
packet_send(packet_t* packet, const net_t* net) {
    uint8_t packet_buf[PACKET_SIZE_MAX] = {0};

    packet_seal(packet);
    packet_encode(packet, packet_buf);

    error_log(
"sent packet with: \n\
magic: %s, usrname: %s, \n\
payld_len: %u, flags: %u, crc: %u, options: %s, \n\
nonce: %lu, timestamp: %lu, \n\
payld: %s",
            packet->magic, packet->usrname,
            packet->payld_len, packet->flags, packet->crc, packet->options,
            packet->nonce, packet->timestamp,
            packet->payld);

    return packet_sendall(net, packet_buf);
}

int
packet_recv(packet_t* packet, const net_t* net) {
    uint8_t packet_buf[PACKET_SIZE_MIN] = {0};

    int bytes = packet_recvhead(net, packet_buf);

    if (bytes <= 0) {
        return bytes;
    }

    packet_decode(packet, packet_buf);

    if (strcmp(PACKET_MAGIC, packet->magic) != 0) {
        error_log("packet err: wrong magic");
        return -1;
    }

    if (packet->payld_len > 0) {
        bytes = packet_recvpayld(net, packet->payld, packet->payld_len);
    }

    uint32_t crc = crc32_generate(packet->payld, packet->payld_len);

    if (packet->crc != crc) {
        error_log("packet err: wrong crc");
        return -1;
    }

    error_log(
"received packet of %d bytes: \n\
magic: %s, usrname: %s, \n\
payld_len: %u, flags: %u, crc: %u, options: %s, \n\
nonce: %lu, timestamp: %lu, \n\
payld: %s",
            bytes,
            packet->magic, packet->usrname,
            packet->payld_len, packet->flags, packet->crc, packet->options,
            packet->nonce, packet->timestamp,
            packet->payld);

    return bytes;
}
