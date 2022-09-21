#include "FlashInfo.h"

using namespace Fs;

/**
 * @brief Identify a flash
 *
 * Given the three byte JEDEC identify response, get information about a flash chip, such as its
 * size and capabilities.
 *
 * @param jedecId JEDEC flash ID string (from command $9f)
 * @param outInfo Variable to receive flash info address, if known
 *
 * @return Whether the flash was identified or not
 */
bool Fs::IdentifyFlash(etl::span<const uint8_t, 3> jdecId, const FlashInfo* &outInfo) {
    // Winbond
    if(jdecId[0] == 0xef) {
        // W25Q64JV-IQ/JQ
        if(jdecId[1] == 0x40 && jdecId[2] == 0x17) {
            static const FlashInfo info{
                .manufacturerName       = "Winbond",
                .partNumber             = "W25Q64JV-IQ/JQ",
                .capacity               = 26,
                .pageSize               = 8,
                .sectorSize             = 12,
                .blockSize              = 16,
                .numSecurityRegisters   = 3,

                .cmdReadStatus          = 0x05,
                .statusBusyBit          = 0b00000001,

                .cmdWriteEnable         = 0x06,
                .cmdWriteDisable        = 0x04,

                .cmdReadSecurity        = 0x48,
                .cmdWriteSecurity       = 0x42,
                .cmdEraseSecurity       = 0x44,

                .cmdRead                = 0x03,
                .cmdFastRead            = 0x0B,
                .cmdProgramPage         = 0x02,
                .cmdEraseSector         = 0x20,
                .cmdEraseBlock          = 0xD8,
                .cmdEraseChip           = 0xC7,

                .cmdPowerDown           = 0xB9,
                .cmdWakeUp              = 0xAB,
                .cmdResetEnable         = 0x66,
                .cmdReset               = 0x99,

                .timeoutPageProgram     = 3,
                .timeoutSectorErase     = 400,
                .timeoutBlockErase      = 2000,
                .timeoutChipErase       = (100 * 1000),
            };
            outInfo = &info;

            return true;
        }
    }

    // failed to find flash
    return false;
}
