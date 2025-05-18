
#include "crc32.h"

#include <stdint.h>
#include <stddef.h>

/*** data ***/

#define CRC32_POLYNOMIAL 0xEDB88320

#define CRC32_BYTE(value) \
    (((value) & 1) ? (((value) >> 1) ^ CRC32_POLYNOMIAL) : ((value) >> 1))

#define CRC32_BYTE_ITER0(value) (value)
#define CRC32_BYTE_ITER1(value) CRC32_BYTE_ITER0(CRC32_BYTE((value)))
#define CRC32_BYTE_ITER2(value) CRC32_BYTE_ITER1(CRC32_BYTE((value)))
#define CRC32_BYTE_ITER3(value) CRC32_BYTE_ITER2(CRC32_BYTE((value)))
#define CRC32_BYTE_ITER4(value) CRC32_BYTE_ITER3(CRC32_BYTE((value)))
#define CRC32_BYTE_ITER5(value) CRC32_BYTE_ITER4(CRC32_BYTE((value)))
#define CRC32_BYTE_ITER6(value) CRC32_BYTE_ITER5(CRC32_BYTE((value)))
#define CRC32_BYTE_ITER7(value) CRC32_BYTE_ITER6(CRC32_BYTE((value)))
#define CRC32_BYTE_ITER8(value) CRC32_BYTE_ITER7(CRC32_BYTE((value)))

#define CRC32_TABLE_ENTRY(idx) CRC32_BYTE_ITER8((idx))

#define CRC32_TABLE_ROW(base) \
    CRC32_TABLE_ENTRY((base + 0)), CRC32_TABLE_ENTRY((base + 1)), \
    CRC32_TABLE_ENTRY((base + 2)), CRC32_TABLE_ENTRY((base + 3)), \
    CRC32_TABLE_ENTRY((base + 4)), CRC32_TABLE_ENTRY((base + 5)), \
    CRC32_TABLE_ENTRY((base + 6)), CRC32_TABLE_ENTRY((base + 7))

static const uint32_t crc32_table[256] = {
    CRC32_TABLE_ROW(0x00), CRC32_TABLE_ROW(0x08), CRC32_TABLE_ROW(0x10), CRC32_TABLE_ROW(0x18),
    CRC32_TABLE_ROW(0x20), CRC32_TABLE_ROW(0x28), CRC32_TABLE_ROW(0x30), CRC32_TABLE_ROW(0x38),
    CRC32_TABLE_ROW(0x40), CRC32_TABLE_ROW(0x48), CRC32_TABLE_ROW(0x50), CRC32_TABLE_ROW(0x58),
    CRC32_TABLE_ROW(0x60), CRC32_TABLE_ROW(0x68), CRC32_TABLE_ROW(0x70), CRC32_TABLE_ROW(0x78),
    CRC32_TABLE_ROW(0x80), CRC32_TABLE_ROW(0x88), CRC32_TABLE_ROW(0x90), CRC32_TABLE_ROW(0x98),
    CRC32_TABLE_ROW(0xA0), CRC32_TABLE_ROW(0xA8), CRC32_TABLE_ROW(0xB0), CRC32_TABLE_ROW(0xB8),
    CRC32_TABLE_ROW(0xC0), CRC32_TABLE_ROW(0xC8), CRC32_TABLE_ROW(0xD0), CRC32_TABLE_ROW(0xD8),
    CRC32_TABLE_ROW(0xE0), CRC32_TABLE_ROW(0xE8), CRC32_TABLE_ROW(0xF0), CRC32_TABLE_ROW(0xF8)
};


/*** generate ***/

uint32_t
crc32_generate(const char *buffer, size_t size) {
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < size; i++) {
            crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buffer[i]];
        }

        return crc ^ 0xFFFFFFFF;
}
