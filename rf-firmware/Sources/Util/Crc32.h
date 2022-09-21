#ifndef UTIL_CRC32_H
#define UTIL_CRC32_H

#include <stddef.h>
#include <stdint.h>

#include <etl/array.h>

namespace Util {
/**
 * @brief Compute the CRC32 of a byte string
 *
 * Calculate the CRC32 (using the 802.3 polynomail 0x04C11DB7) of the entire provided message.
 */
static inline uint32_t Crc32(etl::span<const uint8_t> message) {
    uint32_t byte, crc{0xFFFFFFFF}, mask;

    for(size_t i = 0; i < message.size(); i++) {
        byte = message[i];
        crc = crc ^ byte;
        for(int j = 7; j>= 0; j--) {
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }

    return ~crc;
}
}

#endif
