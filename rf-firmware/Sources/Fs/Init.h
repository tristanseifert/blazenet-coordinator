#ifndef FS_INIT_H
#define FS_INIT_H

#include <stdint.h>

/// External flash filesystem support
namespace Fs {
/**
 * @brief Flash filesystem superblock
 *
 * This structure is stored in the first sector of the flash, at address 0, and defines the layout
 * of the rest of the memory. It also provides information about the identity of the device (such
 * as its key material, MAC addresses, etc.) among other things.
 */
struct Superblock {
    /// Supported filesystem types
    enum FsType: uint32_t {
        SPIFFS                                  = 0x01,
    };

    /// Header magic value
    constexpr static const uint32_t kMagic{0x424C415A};
    /// Current superblock version
    constexpr static const uint32_t kVersion{0x00000100};

    /**
     * @brief Superblock magic value
     *
     * This should always be 0x424C415A.
     */
    uint32_t magic;

    /**
     * @brief Superblock version
     *
     * Defines the version of the superblock.
     */
    uint32_t version;

    /**
     * @brief Superblock length
     *
     * Total size of the superblock, in bytes, including the trailing checksum.
     */
    uint32_t totalLength;

    /**
     * @brief Filesystem type
     *
     * Defines the type of filesystem that's stored in the SPI NOR.
     *
     * @seeAlso FsType
     */
    uint32_t fsType;

    /**
     * @brief Starting byte address of the filesystem
     *
     * @remark This should be aligned to one of the flash chip's erase block sizes, typically a
     *         single sector.
     */
    uint32_t fsStart;

    /**
     * @brief Ending byte address of the filesystem
     */
    uint32_t fsEnd;

    /**
     * @brief CRC32 over superblock contents
     *
     * This field contains a CRC32 (using the 802.3 Ethernet polynomial 0x04C11DB7) over all
     * previous bytes in the superblock.
     */
    uint32_t crc;
};

void Init();
}

#endif
