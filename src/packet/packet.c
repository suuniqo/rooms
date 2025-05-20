
#include "packet.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../crypto/crc/crc32.h"
#include "../error/error.h"
#include "../serialize/serialize.h"
#include "../syscall/syscall.h"


/*** data ***/

static const char* const PACKET_MAGIC = "rooms";


/*** encoding ***/

static void
packet_seal(packet_t* packet, uint8_t flags) {
    time_t tm = time(NULL);

    if (tm < 0) {
        error_shutdown("packet err: time");
    }

    packet->flags = flags;
    packet->nonce += 1;
    packet->timestamp = (uint64_t)tm;

    if (flags & PACKET_FLAG_MSG) {
        packet->payld_len = (uint8_t)strlen(packet->payld);
        packet->crc = crc32_generate(packet->payld, packet->payld_len);
    } else {
        packet->payld_len = 0;
        packet->crc = 0;
    }
}

static void
packet_encode(const packet_t* packet, uint8_t* buf) {
    pack_str(buf, PACKET_MAGIC);                        buf += SIZE_MAGIC;
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
                                                        buf += SIZE_MAGIC;
    unpack_str(packet->usrname, buf);                   buf += SIZE_USRNAME;

    unpack_chr(&packet->payld_len, buf);                buf += SIZE_PAYLD_LEN;
    unpack_chr(&packet->flags, buf);                    buf += SIZE_FLAGS;
    unpack_u32(&packet->crc, buf);                      buf += SIZE_CRC;
    unpack_str(packet->options, buf);                   buf += SIZE_OPTIONS;

    unpack_u64(&packet->nonce, buf);                    buf += SIZE_NONCE;
    unpack_u64(&packet->timestamp, buf);                buf += SIZE_TIMESTAMP;
}


/*** recv packet ***/

static const uint8_t*
magic_find(const uint8_t* noise, const uint8_t* magic, unsigned noise_size, unsigned magic_size) {
    if (magic_size == 0 || noise_size < magic_size) {
        return NULL;
    }

    for (unsigned i = 0; i <= noise_size - magic_size; ++i) {
        if (memcmp(noise + i, magic, magic_size) == 0) {
            return noise + i;
        }
    }

    return NULL;
}

static int
packet_recvhead(int sockfd, uint8_t* packet_buf) {
    int bytes = 0;

    const uint8_t* magic_pos = NULL;

    bytes = recvall(sockfd, packet_buf, PACKET_SIZE_MIN);

    if (bytes <= 0) {
        return bytes;
    }

    magic_pos = magic_find(packet_buf, (const uint8_t*)PACKET_MAGIC, PACKET_SIZE_MIN, SIZE_MAGIC);

    while (magic_pos == NULL) {
        memmove(packet_buf, packet_buf + PACKET_SIZE_MIN - (SIZE_MAGIC - 1), SIZE_MAGIC - 1);

        bytes = recvall(sockfd, packet_buf + SIZE_MAGIC - 1, PACKET_SIZE_MIN - (SIZE_MAGIC - 1));

        if (bytes <= 0) {
            return bytes;
        }

        magic_pos = magic_find(packet_buf, (const uint8_t*)PACKET_MAGIC, PACKET_SIZE_MIN, SIZE_MAGIC);
    }

    unsigned noise_size = (unsigned)(magic_pos - packet_buf);
    unsigned valid_bytes = PACKET_SIZE_MIN - noise_size;

    if (noise_size > 0) {
        memmove(packet_buf, magic_pos, valid_bytes);
        bytes = recvall(sockfd, packet_buf + valid_bytes, noise_size);

        if (bytes <= 0) {
            return bytes;
        }
    }

    return PACKET_SIZE_MIN;
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
packet_send(packet_t* packet, int sockfd, uint8_t flags) {
    uint8_t packet_buf[PACKET_SIZE_MAX] = {0};

    packet_seal(packet, flags);
    packet_encode(packet, packet_buf);

    return sendall(sockfd, packet_buf, PACKET_SIZE_MIN + packet_buf[OFFSET_PAYLD_LEN]);
}

int
packet_recv(packet_t* packet, int sockfd) {
    uint8_t packet_buf[PACKET_SIZE_MIN] = {0};

    int bytes_head = 0;
    int bytes_payld = 0;

    memset(packet, 0, sizeof(packet_t));

    bytes_head = packet_recvhead(sockfd, packet_buf);

    if (bytes_head <= 0) {
        return bytes_head;
    }

    packet_decode(packet, packet_buf);

    if (__builtin_popcount(packet->flags) != 1) {
        error_log("packet err: wrong flags");
        return -2;
    }

    if (packet->payld_len == 0) {
        return bytes_head;
    }

    bytes_payld = recvall(sockfd, (uint8_t*)packet->payld, packet->payld_len);

    if (bytes_payld <= 0) {
        return bytes_payld;
    }

    uint32_t crc = crc32_generate(packet->payld, packet->payld_len);

    if (packet->crc != crc) {
        error_log("packet err: wrong crc");
        return -2;
    }

    return bytes_head + bytes_payld;
}
