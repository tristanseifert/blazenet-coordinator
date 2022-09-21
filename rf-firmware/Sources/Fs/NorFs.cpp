#include <string.h>
#include <spiffs.h>

#include "Log/Logger.h"

#include "Flash.h"
#include "FlashInfo.h"
#include "Init.h"
#include "NorFs.h"

using namespace Fs;

/**
 * @brief Flash instance used for the NOR FS
 */
Flash *NorFs::gFlash{nullptr};

/**
 * @brief Global SPIFFS instance
 *
 * This is the SPIFFS instance that all file IO goes through.
 */
struct spiffs_t NorFs::gFs;

/**
 * @brief SPIFFs configuration
 *
 * Holds the configuration of the filesystem, including information such as the memory size and
 * IO routines.
 */
static spiffs_config gFsConfig;



/**
 * @brief Format the NOR filesystem
 *
 * Create the SPIFFS partition in the area indicated by the superblock.
 */
int NorFs::Format(Flash *flash, Superblock *super) {
    int err;

    // mount the fs, but expect SPIFFS_ERR_NOT_A_FS error
    err = Mount(flash, super);
    if(err && err != SPIFFS_ERR_NOT_A_FS) {
        return err;
    } else if(!err) {
        // do not re-format an already formatted fs!
        return Error::AlreadyFormatted;
    }

    // format the fs
    err = SPIFFS_format(&gFs);
    return err;
}

/**
 * @brief Initialize the NOR filesystem
 *
 * Attempt to mount the SPIFFS partition in external flash.
 */
int NorFs::Mount(Flash *flash, Superblock *super) {
    int err;
    uint8_t *work{nullptr}, *cache{nullptr};
    size_t cacheSize{0};

    constexpr static const size_t kFdsSize{48 * 8};
    static etl::array<uint8_t, kFdsSize> fds;

    // update config and allocate buffers
    InitFsConfig(flash, super);

    work = reinterpret_cast<uint8_t *>(malloc(2 * gFsConfig.log_page_size));
    if(!work) {
        goto outofmem;
    }

    cacheSize = (gFsConfig.log_page_size + 32) * 4;
    cache = reinterpret_cast<uint8_t *>(malloc(cacheSize));
    if(!cache) {
        Logger::Warning("couldn't alloc %u bytes fs cache", cacheSize);
        cacheSize = 0;
    }

    // attempt to mount fs
    err = SPIFFS_mount(&gFs, &gFsConfig, work, fds.data(), fds.size(), cache, cacheSize, nullptr);
    if(err) {
        free(work);
        free(cache);
    }

    return err;

    // jump here if out of memory
outofmem:;
    if(work) {
        free(work);
    }
    if(cache) {
        free(cache);
    }
    return Error::OutOfMemory;
}



/**
 * @brief Fill in SPIFFS config from superblock
 *
 * Extract the requisite information from the superblock and set up the SPIFFS config structure
 * with that information.
 *
 * @param flash Flash chip the filesystem lives on
 * @param block Superblock read from flash
 */
void NorFs::InitFsConfig(Flash *flash, Superblock *block) {
    // initialize the structs and some state
    memset(&gFsConfig, 0, sizeof(gFsConfig));

    gFlash = flash;

    // flash geometry
    gFsConfig.phys_size = flash->getInfo()->capacityBytes() - block->fsStart;
    gFsConfig.phys_addr = block->fsStart;
    gFsConfig.phys_erase_block = flash->getInfo()->blockSizeBytes();

    // block sizes
    gFsConfig.log_block_size = flash->getInfo()->blockSizeBytes();
    gFsConfig.log_page_size = flash->getInfo()->pageSizeBytes();

    // define IO routines
    gFsConfig.hal_read_f = [](auto addr, auto size, auto buf) -> int {
        if(kLogFsOps) {
            Logger::Notice("FS read: %u bytes from $%06x (%p)", size, addr, buf);
        }
        return gFlash->read(addr, {buf, size});
    };
    gFsConfig.hal_write_f = [](auto addr, auto size, auto buf) -> int {
        if(kLogFsOps) {
            Logger::Notice("FS write: %u bytes to $%06x (%p)", size, addr, buf);
        }
        return gFlash->write(addr, {buf, size});
    };
    gFsConfig.hal_erase_f = [](auto addr, auto size) -> int {
        if(kLogFsOps) {
            Logger::Notice("FS erase: %u bytes from $%06x", size, addr);
        }
        return gFlash->erase(addr, size);
    };
}
