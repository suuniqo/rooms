
#if !defined(SERIALIZE_H)
#define SERIALIZE_H

#include <stdint.h>

/*** pack ***/

extern void
pack_chr(uint8_t* buf, uint8_t byte);

extern void
pack_str(uint8_t* buf, const char* data);

extern void
pack_u32(uint8_t* buf, uint32_t data);

extern void
pack_u64(uint8_t* buf, uint64_t data);


/*** unpack ***/

extern void
unpack_chr(uint8_t* field, const uint8_t* buf);

extern void
unpack_str(char* field, const uint8_t* buf);

extern void
unpack_u32(uint32_t* field, const uint8_t* buf);

extern void
unpack_u64(uint64_t* field, const uint8_t* buf);

#endif /* !defined(SERIALIZE_H) */
