#ifndef FS_SPIFFS_H
#define FS_SPIFFS_H

namespace Fs {
class Flash;
struct Superblock;

/**
 * @brief NOR Flash filesystem handler
 *
 * Provides a high level interface to the filesystem on the external SPI NOR flash.
 */
class NorFs {
    public:
        /// Error codes from NOR FS
        enum Error: int {
            NoError                             = 0,
            Success                             = NoError,

            /// Failed to allocate some required memory
            OutOfMemory                         = -1100,
            /// The filesystem is already formatted
            AlreadyFormatted                    = -1101,
        };

    private:
        /// Should filesystem ops be logged?
        constexpr static const bool kLogFsOps{false};

    public:
        static int Mount(Flash *flash, Superblock *super);
        static int Format(Flash *flash, Superblock *super);

    private:
        static void InitFsConfig(Flash *flash, Superblock *block);

    private:
        static Flash *gFlash;
        static struct spiffs_t gFs;
};
}

#endif
