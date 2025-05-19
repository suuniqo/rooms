
#include "serialize.h"

#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>


/*** macros ***/

#define U64_HI(VAR) ((VAR) >> 32)
#define U64_LO(VAR) ((VAR) & 0xFFFFFFFF)
#define U64_FROM_HALVES(HIGH, LOW) (((uint64_t)(HIGH) << 32) | (LOW))


/*** pack ***/

void
pack_chr(uint8_t* buf, uint8_t byte) {
    *buf = byte;
}

void
pack_str(uint8_t* buf, const char* data) {
    size_t len = strlen(data);

    memcpy(buf, data, len);
    buf[len] = '\0';
}

void
pack_u32(uint8_t* buf, uint32_t data) {
    uint32_t* buf32 = (uint32_t*)buf;
    *buf32 = htonl(data);
}

void
pack_u64(uint8_t* buf, uint64_t data) {
    uint64_t data64 = data;
    uint32_t high = htonl((uint32_t) U64_HI(data64));
    uint32_t low =  htonl((uint32_t) U64_LO(data64));

    uint32_t* buf32 = (uint32_t*)buf;
    buf32[0] = high;
    buf32[1] = low;
}


/*** unpack ***/

void
unpack_chr(uint8_t* field, const uint8_t* buf) {
    *field = *buf;
}

void
unpack_str(char* field, const uint8_t* buf) {
    size_t len = strlen((const char*)buf);

    memcpy(field, (const char*)buf, len);
    field[len] = '\0';
}

void
unpack_u32(uint32_t* field, const uint8_t* buf) {
    const uint32_t* buf32 = (const uint32_t*)buf;

    *field = ntohl(*buf32);
}

void
unpack_u64(uint64_t* field, const uint8_t* buf) {
    const uint32_t* buf32 = (const uint32_t*)buf;

    uint32_t high = ntohl(buf32[0]);
    uint32_t low = ntohl(buf32[1]);

    *field = U64_FROM_HALVES(high, low);
}
