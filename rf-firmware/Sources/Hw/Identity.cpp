#include <Util/Base32.h>
#include <Util/Hash.h>
#include <em_device.h>

#include "Log/Logger.h"

#include "Identity.h"

using namespace Hw;

etl::array<uint8_t, 8> Identity::gEui64;
etl::array<char, Identity::kSerialMaxLength> Identity::gSerial;

/**
 * @brief Read identity information
 *
 * Read the DEVINFO memory region to get the EUI-64 of the device.
 */
void Identity::Init() {
    uint32_t temp;

    // read out EUI64
    temp = __builtin_bswap32(DEVINFO->EUI64H);
    memcpy(gEui64.data()    , &temp, sizeof(uint32_t));
    temp = __builtin_bswap32(DEVINFO->EUI64L);
    memcpy(gEui64.data() + 4, &temp, sizeof(uint32_t));

    // compute serial number by hashing EUI64
    const auto serialHash = Util::Hash::MurmurHash3(gEui64, kSerialHashSeed);
    Util::Base32::Encode({reinterpret_cast<const uint8_t *>(&serialHash), sizeof(serialHash)},
            gSerial);
}
