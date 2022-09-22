#ifndef HW_IDENTITY_H
#define HW_IDENTITY_H

#include <stddef.h>
#include <stdint.h>
#include <printf/printf.h>
#include <etl/array.h>
#include <etl/span.h>

namespace Hw {
/**
 * @brief Device identity manager
 *
 * Reads device identitfication information (such as the EUI-48 and EUI-64, serial number, etc.)
 * from nonvolatile device registers, and provides it for later use.
 */
class Identity {
    private:
        /// Maximum length of the serial number string (bytes)
        constexpr static const size_t kSerialMaxLength{8};
        /// Hash seed for computing serial number
        constexpr static const uint32_t kSerialHashSeed{'SERN'};

    public:
        static void Init();

        /**
         * @brief Get the EUI-64 address
         */
        constexpr static inline etl::span<const uint8_t, 8> GetEui64() {
            return gEui64;
        }
        /**
         * @brief Get EUI-64 address as string
         */
        static inline void FormatEui64String(etl::span<char> buffer) {
            snprintf(buffer.data(), buffer.size(), "%02x:%02x:%02x:%02x:%02x:%02x",
                    gEui64[0], gEui64[1], gEui64[2], gEui64[3], gEui64[4], gEui64[5]);
        }

        /**
         * @brief Get the serial number string
         */
        static inline const char *GetSerial() {
            return gSerial.data();
        }

    private:
        /// Storage for device's EUI-64
        static etl::array<uint8_t, 8> gEui64;
        /// Storage for device's serial number string (NULL terminated)
        static etl::array<char, kSerialMaxLength> gSerial;
};
}

#endif
