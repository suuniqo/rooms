
#if !defined(CRC32)
#define CRC32

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

/*** generate ***/

uint32_t
crc32_generate(const char* data, size_t size);

#endif // !defined(CRC32)
