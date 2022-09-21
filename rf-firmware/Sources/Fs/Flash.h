#ifndef FS_FLASH_H
#define FS_FLASH_H

#include <stdint.h>

#include <etl/array.h>
#include <etl/span.h>

#include "sl_spidrv_eusart_flash_config.h"

namespace Fs {
struct FlashInfo;

/**
 * @brief Flash access wrapper
 *
 * Provides a simple command based interface to an SPI NOR flash, based on its information
 * structure which defines all of the commands.
 */
class Flash {
    private:
        /// Should flash writes be logged?
        constexpr static const bool kLogWrites{false};
        /// Should flash erases be logged?
        constexpr static const bool kLogErase{false};

    public:
        enum Error: int {
            NoError                             = 0,
            Success                             = NoError,

            /// Timeout waiting for an erase/write to complet
            Timeout                             = -1,
            /// IO Error during command phase
            IoCommand                           = -2,
            /// IO Error during payload phase
            IoPayload                           = -3,
            /// Attempt to erase at an address not a multiple of the erase granularity
            UnalignedAddress                    = -4,
            /// Attempt to write beyond a page boundary
            PageWriteTooBig                     = -5,
            /// Specified arguments are invalid
            InvalidArguments                    = -6,
        };

        Flash(const FlashInfo *info);

        int read(const uintptr_t address, etl::span<uint8_t> buffer);

        int writeEnable();
        int waitForCompletion(const uint32_t timeoutMsec);

        int write(const uintptr_t address, etl::span<const uint8_t> data);
        int writePage(const uintptr_t address, etl::span<const uint8_t> data);

        int eraseSector(const uintptr_t address);
        int eraseBlock(const uintptr_t address);
        int eraseChip();

        int reset();

        /**
         * @brief Get the flash information structure
         */
        constexpr inline auto getInfo() const {
            return this->info;
        }

        /**
         * @brief Execute the "JEDEC Identify" command
         *
         * Read the three byte JEDEC identification string from the chip.
         */
        static int Identify(etl::span<uint8_t, 3> jedecId) {
            etl::array<uint8_t, 1> txBuf{{0x9f}};
            return ExecCmdRead(txBuf, jedecId);
        }

    private:
        /**
         * @brief Submit an erase command
         *
         * Execute the given erase command, and wait for the command to complete.
         */
        inline int eraseWithAddress(const uint8_t cmd, const uintptr_t address,
                const uint32_t timeoutMsec) {
            int err;

            // enable the chip for writing
            err = this->writeEnable();
            if(err) {
                return err;
            }

            // send the command
            etl::array<uint8_t, 4> cmdBuf{{
                cmd, static_cast<uint8_t>((address & 0xFF0000) >> 16),
                static_cast<uint8_t>((address & 0x00FF00) >> 8),
                static_cast<uint8_t>((address & 0x0000FF)),
            }};
            err = ExecCmd(cmdBuf);
            if(err) {
                return err;
            }

            // wait for completion
            return this->waitForCompletion(timeoutMsec);
        }

    private:
        /**
         * @brief Set whether flash chip select is asserted
         */
        static inline void SetCsAsserted(const bool isAsserted) {
            if(isAsserted) {
                GPIO_PinOutClear(SL_SPIDRV_EUSART_FLASH_CS_PORT, SL_SPIDRV_EUSART_FLASH_CS_PIN);
            } else {
                GPIO_PinOutSet(SL_SPIDRV_EUSART_FLASH_CS_PORT, SL_SPIDRV_EUSART_FLASH_CS_PIN);
            }
        }

        /**
         * @brief Execute a command without payload
         */
        static inline int ExecCmd(etl::span<const uint8_t> cmd) {
            etl::span<uint8_t> none;
            return ExecCmdRead(cmd, none);
        }

        /**
         * @brief Execute a command, then read payload
         *
         * Send the given command to the flash, then read back payload. This will use the non-
         * blocking interface if the scheduler has been started.
         */
        static inline int ExecCmdRead(etl::span<const uint8_t> cmd, etl::span<uint8_t> data,
                const bool async = true) {
            return ExecCmdReadBlocking(cmd, data);
        }
        static int ExecCmdReadBlocking(etl::span<const uint8_t>, etl::span<uint8_t>);

        /**
         * @brief Execute a command, then write payload
         *
         * Send the given command to the flash, then write an additional payload. This will
         * use the non- blocking interface if the scheduler has been started.
         */
        static inline int ExecCmdWrite(etl::span<const uint8_t> cmd, etl::span<const uint8_t> data,
                const bool async = true) {
            return ExecCmdWriteBlocking(cmd, data);
        }
        static int ExecCmdWriteBlocking(etl::span<const uint8_t>, etl::span<const uint8_t>);

    private:
        /// Flash information structure
        const FlashInfo *info;
};
}

#endif
