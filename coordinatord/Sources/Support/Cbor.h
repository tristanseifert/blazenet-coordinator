#ifndef SUPPORT_CBOR_H
#define SUPPORT_CBOR_H

#include <cbor.h>

#include <stdexcept>
#include <string_view>

namespace Support {
/**
 * @brief Read a CBOR integer value
 *
 * @param item CBOR item to read
 *
 * @remark The item must be an unsigned integer or the results are undefined.
 */
constexpr inline static uint64_t CborReadUint(const cbor_item_t *item) {
    if(!cbor_isa_uint(item)) {
        throw std::runtime_error("invalid type (expected uint)");
    }

    switch(cbor_int_get_width(item)) {
        case CBOR_INT_8:
            return cbor_get_uint8(item);
        case CBOR_INT_16:
            return cbor_get_uint16(item);
        case CBOR_INT_32:
            return cbor_get_uint32(item);
        case CBOR_INT_64:
            return cbor_get_uint64(item);
    }
}

/**
 * @brief Get the value given a key from a map
 *
 * @param key String key to look up
 *
 * @return A CBOR item if found, nullptr if not
 */
constexpr inline const cbor_item_t *CborMapGet(const cbor_item_t *map,
        const std::string_view &inKey) {
    auto rootKeys = cbor_map_handle(map);

    for(size_t i = 0; i < cbor_map_size(map); i++) {
        auto &pair = rootKeys[i];
        const std::string_view key{reinterpret_cast<const char *>(cbor_string_handle(pair.key)),
            cbor_string_length(pair.key)};

        if(key == inKey) {
            return pair.value;
        }
    }

    return nullptr;
}
}

#endif
