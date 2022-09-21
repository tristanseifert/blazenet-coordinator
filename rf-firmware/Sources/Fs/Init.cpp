#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <etl/array.h>

#include "em_device.h"
#include "em_gpio.h"

#include "Drivers/sl_spidrv_instances.h"
#include "sl_spidrv_eusart_flash_config.h"

#include "Log/Logger.h"
#include "Util/Crc32.h"

#include "Flash.h"
#include "FlashInfo.h"
#include "Init.h"

using namespace Fs;

/**
 * @brief Base address for superblock
 *
 * Flash address at which the superblock is stored
 */
constexpr static const uintptr_t kSuperblockAddress{0x000000};

/**
 * @brief Initialize the contents of a superblock
 *
 * This is used during formatting to set up the filesystem. The superblock is initialized and all
 * remaining space in the filesystem is reserved for the filesystem use.
 */
static void InitSuperblock(Flash *flash, Superblock *superblock) {
    // start with the constants
    superblock->magic = Superblock::kMagic;
    superblock->version = Superblock::kVersion;
    superblock->totalLength = sizeof(Superblock);

    // partitioning for SPIFFS
    superblock->fsType = Superblock::FsType::SPIFFS;

    superblock->fsStart = flash->getInfo()->sectorSizeBytes();
    superblock->fsEnd = flash->getInfo()->capacityBytes() - 1 - superblock->fsStart;

    // calculate CRC
    etl::span<const uint8_t, sizeof(Superblock)> superblockBytes{
        reinterpret_cast<const uint8_t *>(superblock), sizeof(Superblock)};
    const auto superblockCrcBytes = superblockBytes.subspan<0, offsetof(Superblock, crc)>();

    superblock->crc = Util::Crc32(superblockCrcBytes);
}

/**
 * @brief Format the attached SPI flash
 *
 * This will erase the entire chip, then write in the superblock and format the underlying
 * filesystem as well. It will be empty.
 */
static void FormatNor(Flash *flash, Superblock *superblock) {
    int err{0};

    // erase the entire chip
    Logger::Notice("Erasing NOR!");
    //err = flash->eraseChip();
    err = flash->eraseSector(kSuperblockAddress);
    REQUIRE(!err, "%s failed: %d", "erase NOR", err);

    // allocate superblock memory then populate it
    InitSuperblock(flash, superblock);

    // then write the superblock
    Logger::Notice("Writing superblock!");
    etl::span<const uint8_t, sizeof(Superblock)> superblockBytes{
        reinterpret_cast<const uint8_t *>(superblock), sizeof(Superblock)};

    err = flash->write(kSuperblockAddress, superblockBytes);
    REQUIRE(!err, "%s failed: %d", "write superblock", err);

    // TODO: initialize fs
}

/**
 * @brief Wrapper around formatting to reset the system after
 */
[[noreturn]] static inline void Format(Flash *flash, Superblock *superblock) {
    Logger::Warning("NOR is empty, formatting");
    FormatNor(flash, superblock);

    Logger::Notice("Format complete, resetting");
    NVIC_SystemReset();
}



/**
 * @brief Initialize the external filesystem
 *
 * Probe the SPI NOR flash connected to determine its size/type, then read the superblock and parse
 * that to figure out the extents of the filesystem.
 */
void Fs::Init() {
    int err;
    const FlashInfo *info{nullptr};

    // configure SPI CS line GPIO
    GPIO_PinModeSet(SL_SPIDRV_EUSART_FLASH_CS_PORT, SL_SPIDRV_EUSART_FLASH_CS_PIN,
            gpioModePushPull, true);

    /*
     * Identify the flash by sending command 9Fh "Read JEDEC ID." The flash will respond with three
     * bytes of information: manufacturer ID, memory type ID, and capacity ID.
     */
    etl::array<uint8_t, 3> jedecId;
    err = Flash::Identify(jedecId);
    REQUIRE(!err, "%s failed: %d", "Flash::identify", err);

    if(!IdentifyFlash(jedecId, info)) {
        Logger::Panic("Unknown Flash ID: %02x %02x %02x", jedecId[0], jedecId[1], jedecId[2]);
    }
    REQUIRE(!!info, "failed to identify flash");

    Logger::Debug("NOR flash: %s %s %u bytes (%u byte pages, %u byte sectors, %u byte blocks)",
            info->manufacturerName.data(), info->partNumber.data(), info->capacityBytes(),
            info->pageSizeBytes(), info->sectorSizeBytes(), info->blockSizeBytes());

    /*
     * Initialize the flash access driver based on the provided flash info.
     *
     * This is just a thin wrapper that ensures the commands are issued correctly, and implements
     * the blocking interface as well for use when the scheduler is active.
     */
    auto flash = new Flash(info);
    err = flash->reset();
    REQUIRE(!err, "%s failed: %d", "reset flash", err);

    /*
     * Read out (and validate) the superblock from the flash. This will indicate where the actual
     * filesystem begins, and where we can read the identity data from.
     *
     * Also ensure that the flash isn't empty: we check this by comparing the entire superblock
     * against 0xFF. If that's the case, we go immediately to format (and then reset) to handle the
     * first boot case.
     */
    auto superblock = reinterpret_cast<Superblock *>(malloc(sizeof(Superblock)));
    REQUIRE(!!superblock, "failed to alloc %s", "flash superblock");

    err = flash->read(kSuperblockAddress,
            {reinterpret_cast<uint8_t *>(superblock), sizeof(Superblock)});
    REQUIRE(!err, "%s failed: %d", "read superblock", err);

    // check if the entire superblock is 0xFF (erased) so we can format it
    bool isEmpty{true};
    for(size_t i = 0; i < sizeof(Superblock); i++) {
        if(reinterpret_cast<uint8_t *>(superblock)[i] != 0xFF) {
            isEmpty = false;
            break;
        }
    }

    if(isEmpty) {
        Format(flash, superblock);
    }

    /*
     * We've read _something_ that may be a superblock, e.g. the flash isn't empty.
     *
     * Perform some validation over the structure. First validate that the length value is
     * sensible, then we'll check the magic value, CRC, version, and other such fun things before
     * we go and do stuff with it.
     *
     * If any part of this validation fails, we'll panic, as this most likely means the flash has
     * become corrupted.
     */
    bool superblockValid{false};
    // calculate the CRC over the read bytes (assuming the struct version matches what we've got)
    etl::span<const uint8_t, sizeof(Superblock)> superblockBytes{
        reinterpret_cast<const uint8_t *>(superblock), sizeof(Superblock)};
    const auto superblockCrcBytes = superblockBytes.subspan<0, offsetof(Superblock, crc)>();

    const auto computedCrc = Util::Crc32(superblockCrcBytes);

    // before accessing it, ensure the size is sensible
    if(superblock->totalLength < sizeof(Superblock)) {
        Logger::Warning("invalid superblock %s: %08x (expected %08x)", "size",
                superblock->totalLength, sizeof(Superblock));
    }
    else if(superblock->magic != Superblock::kMagic) {
        Logger::Warning("invalid superblock %s: %08x (expected %08x)", "magic", superblock->magic,
                Superblock::kMagic);
    } else if(superblock->crc != computedCrc) {
        Logger::Warning("invalid superblock %s: %08x (expected %08x)", "CRC", superblock->crc,
                computedCrc);
    } else {
        // otherwise, it passed all the tests
        superblockValid = true;
    }

    REQUIRE(superblockValid, "flash superblock is invalid!");

    // otherwise, initialize the filesystem

    Logger::Notice("Superblock (version %08x)", superblock->version);
    // TODO: initialize filesystem

    free(superblock);
}
