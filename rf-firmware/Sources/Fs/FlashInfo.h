#ifndef FS_FLASHINFO_H
#define FS_FLASHINFO_H

#include <stddef.h>
#include <stdint.h>

#include <etl/optional.h>
#include <etl/span.h>
#include <etl/string_view.h>

#include <math.h>

namespace Fs {
/**
 * @brief Information about a flash chip
 *
 * Information structure that defines information about a flash chip.
 *
 * @remark Only power of two sizes for the flash and its pages, sectors and blocks are supported.
 */
struct FlashInfo {
    /// Name of the manufacturer of this chip
    etl::string_view manufacturerName;
    /// Model string of the chip
    etl::string_view partNumber;

    /// Capacity (log2 bytes)
    uint8_t capacity;
    /// Size of a single page (log2 bytes)
    uint8_t pageSize;
    /// Size of a single sector (log2 bytes)
    uint8_t sectorSize;
    /// Size of a block (log2 bytes)
    uint8_t blockSize;

    /// Number of security registers
    uint8_t numSecurityRegisters;

    /// Command to read the primary status register (for checking completion)
    uint8_t cmdReadStatus;
    /// Bit inside the status register indicating flash is busy
    uint8_t statusBusyBit;

    /// Command to enable writes
    uint8_t cmdWriteEnable;
    /// Command to disable writes
    uint8_t cmdWriteDisable;

    /// Command to read security registers
    uint8_t cmdReadSecurity;
    /// Command to write security register
    uint8_t cmdWriteSecurity;
    /// Command to erase security register
    uint8_t cmdEraseSecurity;

    /// Command to perform a regular (low speed) read
    uint8_t cmdRead;
    /// Command to perform a fast (high-speed, with dummy cycle) read
    uint8_t cmdFastRead;
    /// Command to program a page
    uint8_t cmdProgramPage;
    /// Command to erase a sector
    uint8_t cmdEraseSector;
    /// Command to erase a block
    uint8_t cmdEraseBlock;
    /// Command to erase the entire chip
    uint8_t cmdEraseChip;

    /// Command to enter low power mode
    uint8_t cmdPowerDown;
    /// Command to release the device from low power mode
    uint8_t cmdWakeUp;

    /// Command required to enable resetting the device (set to 0 if not required)
    uint8_t cmdResetEnable;
    /// Command to reset the device
    uint8_t cmdReset;

    /// Page program timeout (msec)
    uint32_t timeoutPageProgram;
    /// Sector erase timeout (msec)
    uint32_t timeoutSectorErase;
    /// Block erase timeout (msec)
    uint32_t timeoutBlockErase;
    /// Chip erase timeout (msec)
    uint32_t timeoutChipErase;


    /**
     * @brief Get the capacity, in bytes
     */
    constexpr inline size_t capacityBytes() const {
        return (1ULL << this->capacity);
    }

    /**
     * @brief Get the page size, in bytes
     */
    constexpr inline size_t pageSizeBytes() const {
        return (1ULL << this->pageSize);
    }

    /**
     * @brief Get the sector size, in bytes
     */
    constexpr inline size_t sectorSizeBytes() const {
        return (1ULL << this->sectorSize);
    }

    /**
     * @brief Get the block size, in bytes
     */
    constexpr inline size_t blockSizeBytes() const {
        return (1ULL << this->blockSize);
    }
};

bool IdentifyFlash(etl::span<const uint8_t, 3> jdecId, const FlashInfo* &outInfo);
}

#endif
