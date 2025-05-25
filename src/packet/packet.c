
#include "packet.h"

#include <stdbool.h>
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
#include "../log/log.h"
#include "../serialize/serialize.h"
#include "../syscall/syscall.h"


/*** data ***/

static const char* const PACKET_MAGIC = "rooms";


/*** encoding ***/

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


/*** validation ***/

#define PACKET_HAS_PAYLD(packet) ((packet)->flags & (PACKET_FLAG_MSG | PACKET_FLAG_WHSP))

static bool
packet_valstr(const char* str) {
    size_t len = strlen(str);

    for (size_t i = len; i < sizeof(str); ++i) {
        if (str[i] != '\0') {
            return false;
        }
    }

    return true;
}

static bool
packet_valhead(packet_t* packet) {
    if (strlen(packet->usrname) == 0) {
        log_error("packet err: empty usrname");
        return false;
    }

    if (__builtin_popcount(packet->flags) != 1) {
        log_error("packet err: corrupt flags 0x%02x", packet->flags);
        return false;
    }

    if (PACKET_HAS_PAYLD(packet) && (packet->payld_len == 0 || packet->crc == 0)) {
        log_error("packet err: incoherent contents (type = MSG/WHSP and PAYLD_LEN = 0)");
        return false;
    }

    if (!PACKET_HAS_PAYLD(packet) && (packet->payld_len != 0 || packet->crc != 0)) {
        log_error("packet err: incoherent contents (type != MSG/WHSP and PAYLD_LEN != 0)");
        return false;
    }

    if (packet->flags & PACKET_FLAG_WHSP && strlen(packet->options) == 0) {
        log_error("packet err: incoherent contents (type = WHSP and OPTIONS_LEN = 0)");
        return false;
    }

    if (!(packet->flags & PACKET_FLAG_WHSP) && strlen(packet->options) != 0) {
        log_error("packet err: incoherent contents (type != WHSP and OPTIONS_LEN != 0)");
        return false;
    }

    if (!packet_valstr(packet->usrname)) {
        log_error("packet err: corrupt usrname (not sanitized)");
        return false;
    }

    if (!packet_valstr(packet->options)) {
        log_error("packet err: corrupt options (not sanitized)");
        return false;
    }

    return true;
}

static bool
packet_valpayld(packet_t* packet) {
    uint32_t crc = crc32_generate(packet->payld, packet->payld_len);

    if (packet->crc != crc) {
        log_error("packet err: corrupt crc, (expected %08x, got %08x)");
        return false;
    }

    return true;
}

/*** recv ***/

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
packet_recvhead(int sockfd, uint8_t* packet_buf, int max_resyncs) {
    int bytes = 0;

    const uint8_t* magic_pos = NULL;

    bytes = recvall(sockfd, packet_buf, PACKET_SIZE_MIN);

    if (bytes <= 0) {
        return bytes;
    }

    magic_pos = magic_find(packet_buf, (const uint8_t*)PACKET_MAGIC, PACKET_SIZE_MIN, SIZE_MAGIC);

    for (int resync = 0; magic_pos == NULL; ++resync) {
        if (max_resyncs >= 0 && resync > max_resyncs) {
            return RECV_DESYNC;
        }

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

void
packet_seal(packet_t* packet, uint8_t flags) {
    time_t tm = time(NULL);

    if (tm < 0) {
        log_shutdown("packet err: time");
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

int
packet_send(const packet_t* packet, int sockfd) {
    uint8_t packet_buf[PACKET_SIZE_MAX] = {0};

    packet_encode(packet, packet_buf);

    return sendall(sockfd, packet_buf, PACKET_SIZE_MIN + packet_buf[OFFSET_PAYLD_LEN]);
}

int
packet_recv(packet_t* packet, int sockfd, int max_resyncs) {
    uint8_t packet_buf[PACKET_SIZE_MIN] = {0};

    int bytes_head = 0;
    int bytes_payld = 0;

    memset(packet, 0, sizeof(packet_t));

    bytes_head = packet_recvhead(sockfd, packet_buf, max_resyncs);

    if (bytes_head <= 0) {
        return bytes_head;
    }

    packet_decode(packet, packet_buf);

    if (!packet_valhead(packet)) {
        return RECV_INVAL;
    }

    if (packet->payld_len == 0) {
        return bytes_head;
    }

    bytes_payld = recvall(sockfd, (uint8_t*)packet->payld, packet->payld_len);

    if (bytes_payld <= 0) {
        return bytes_payld;
    }

    if (!packet_valpayld(packet)) {
        return RECV_INVAL;
    }

    return bytes_head + bytes_payld;
}

packet_t
packet_build_ping(const char* usrname) {
    packet_t packet = {0};

    packet.flags = PACKET_FLAG_PING;
    memcpy(packet.usrname, usrname, strlen(usrname));

    return packet;
}

void
packet_build_pong(packet_t* ping) {
    ping->flags = PACKET_FLAG_PONG;
}

void
packet_build_ack(packet_t* packet) {
    packet->flags = PACKET_FLAG_ACK;
    packet->payld_len = 0;
    packet->crc = 0;

    memset(packet->payld, 0, sizeof(packet->payld));
}

