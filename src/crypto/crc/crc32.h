
#if !defined(CRC32_H)
#define CRC32_H

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

/*** generate ***/

extern uint32_t
crc32_generate(const char* data, size_t size);

#endif /* !defined(CRC32_H) */
