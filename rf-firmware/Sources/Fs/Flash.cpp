#include <stddef.h>
#include <stdint.h>

#include "Drivers/sl_spidrv_instances.h"
#include "Log/Logger.h"

#include "Flash.h"
#include "FlashInfo.h"

using namespace Fs;

/**
 * @brief Execute the JEDEC "identify" command and return its results
 *
 * @param jedecId Buffer to receive the three byte JEDEC ID
 */
/*

    GPIO_PinOutClear(SL_SPIDRV_EUSART_FLASH_CS_PORT, SL_SPIDRV_EUSART_FLASH_CS_PIN);
    ok = SPIDRV_MTransferB(sl_spidrv_eusart_flash_handle, txBuf.data(), rxBuf.data(),
            txBuf.size());
    GPIO_PinOutSet(SL_SPIDRV_EUSART_FLASH_CS_PORT, SL_SPIDRV_EUSART_FLASH_CS_PIN);

    REQUIRE(ok == ECODE_EMDRV_SPIDRV_OK, "%s failed: %d", "SPIDRV_MTransfer", ok);
    */

/**
 * @brief Initialize the flash wrapper instance
 *
 * @param info Flash information descriptor
 */
Flash::Flash(const FlashInfo *info) : info(info) {
    // TODO: is there anything to do here?
}

/**
 * @brief Read flash memory
 *
 * Start reading out the flash memory at the specified logical address.
 *
 * @param address Memory address to begin reading at
 * @param buffer Memory region to receive the read data
 */
int Flash::read(const uintptr_t address, etl::span<uint8_t> buffer) {
    // validate inputs
    if(buffer.empty()) {
        return Error::InvalidArguments;
    }

    // perform the read
    etl::array<uint8_t, 4> cmd{{
        this->info->cmdRead, static_cast<uint8_t>((address & 0xFF0000) >> 16),
            static_cast<uint8_t>((address & 0x00FF00) >> 8),
            static_cast<uint8_t>((address & 0x0000FF)),
    }};
    return ExecCmdRead(cmd, buffer);
}

/**
 * @brief Enable writing to the flash
 */
int Flash::writeEnable() {
    // bail if this command is not supported
    if(!this->info->cmdWriteEnable) {
        return Error::NoError;
    }

    etl::array<uint8_t, 1> cmd{{
        this->info->cmdWriteEnable
    }};
    return ExecCmd(cmd);
}

/**
 * @brief Poll the chip for completion
 *
 * Read the status register of the flash device, and inspect the busy bit. Repeat this process
 * until the chip is either no longer busy or we time out.
 *
 * @remark The timeout does not function while the scheduler is not yet started.
 *
 * @param timeoutMsec Maximum time for the operation, in msec, before aborting
 */
int Flash::waitForCompletion(const uint32_t timeoutMsec) {
    int err;

    // command for reading status register
    etl::array<uint8_t, 1> cmd{{
        this->info->cmdReadStatus
    }};
    etl::array<uint8_t, 1> status{{0xff}};

    // TODO: how does this handle wrap-around?
    const auto now = xTaskGetTickCount();
    const auto expires = now + pdMS_TO_TICKS(timeoutMsec);
    const bool withDelay = !(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED);

    while(true) {
        // check if no longer busy
        err = ExecCmdRead(cmd, status);
        if(err) {
            return err;
        }

        if(!(status[0] & this->info->statusBusyBit)) {
            // no longer busy :)
            return Error::NoError;
        }

        // check for timeout
        if(xTaskGetTickCount() > expires) {
            return Error::Timeout;
        }

        // wait some time before trying again
        if(withDelay) {
            // TODO: use the smaller of 10ms or the remaining timeout
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            volatile unsigned int blah{420'690};
            while(blah--) {}
        }
    }
}

/**
 * @brief Program flash
 *
 * Break the specified continuous program operation into one or more page sized program operations
 * that the flash can execute natively.
 */
int Flash::write(const uintptr_t address, etl::span<const uint8_t> data) {
    int err;

    if(kLogWrites) {
        Logger::Notice("Write(%06x): %u bytes from %p", address, data.size(), data.data());
    }

    // validate inputs
    if(data.empty()) {
        return Error::InvalidArguments;
    }

    // loop until all bytes are written
    size_t totalBytes = data.size(), offset{0};
    const auto pageSize = this->info->pageSizeBytes();

    while(totalBytes) {
        // what's the largest piece of a page we can write?
        const auto start = address + offset;
        const auto numBytes = etl::min(pageSize - (start & (pageSize - 1)), totalBytes);

        // do the write
        err = this->writePage(start, { data.data() + offset, numBytes });
        if(err) {
            return err;
        }

        // update bookkeeping
        offset += numBytes;
        totalBytes -= numBytes;
    }

    // if we get here, we ostensibly finished writing every page with success :D
    return Error::NoError;
}

/**
 * @brief Program up to a page of flash
 *
 * Write data to at most a single page of flash. The write must fit entirely inside the
 * confines of a page.
 *
 * @remark The pages being written to must have already been erased: programming can only set a 1
 *         bit to 0.
 *
 * @param address Page address to begin writing at
 * @param data Data to write to the page (up to page size)
 */
int Flash::writePage(const uintptr_t address, etl::span<const uint8_t> data) {
    int err;

    if(kLogWrites) {
        Logger::Notice("PageWrite(%06x): %u bytes from %p", address, data.size(), data.data());
    }

    // validate inputs
    if(data.empty()) {
        return Error::InvalidArguments;
    }

    const size_t wrapBytes = data.size() + (address & 0x0000FF);
    if(wrapBytes > this->info->pageSizeBytes()) {
        // TODO: validate the above condition hasn't got an off-by-1
        return Error::PageWriteTooBig;
    }

    // enable for writing…
    err = this->writeEnable();
    if(err) {
        return err;
    }

    // …then do the actual write
    etl::array<uint8_t, 4> cmdBuf{{
        this->info->cmdProgramPage, static_cast<uint8_t>((address & 0xFF0000) >> 16),
        static_cast<uint8_t>((address & 0x00FF00) >> 8),
        static_cast<uint8_t>((address & 0x0000FF)),
    }};

    err = ExecCmdWrite(cmdBuf, data);
    if(err) {
        return err;
    }

    // and wait for the program operation to complete
    return this->waitForCompletion(this->info->timeoutPageProgram);
}

/**
 * @brief Erase part of the flash
 *
 * Erases a section of the flash, starting at the given address. Both the address and length must
 * be aligned on the smallest erase granularity (sector) boundary. We'll automagically try to use
 * more efficient block erase commands if the size is large.
 */
int Flash::erase(const uintptr_t address, const size_t length) {
    int err;

    if(kLogErase) {
        Logger::Notice("Erase(%06x) %u bytes", address, length);
    }

    // ensure everything is aligned to a sector boundary
    if(address & (this->info->sectorSizeBytes() - 1)) {
        return Error::UnalignedAddress;
    }
    else if(length & (this->info->sectorSizeBytes() - 1)) {
        return Error::UnalignedSize;
    }

    // loop until all bytes are erased
    size_t totalBytes = length, offset{0};
    const auto sectorSize = this->info->sectorSizeBytes(),
          blockSize = this->info->blockSizeBytes();

    while(totalBytes) {
        const auto start = address + offset;

        // erase sector if not block aligned, or less than a block remains
        if((start & (blockSize - 1)) || totalBytes < blockSize) {
            err = this->eraseSector(start);
            if(err) {
                return err;
            }

            offset += sectorSize;
            totalBytes -= sectorSize;
        }
        // otherwise, erase an entire block
        else {
            err = this->eraseBlock(start);
            if(err) {
                return err;
            }

            offset += blockSize;
            totalBytes -= blockSize;
        }
    }

    // successfully finished erasing
    return Error::NoError;
}

/**
 * @brief Erase a sector
 *
 * Erase a sector starting at the given address.
 *
 * @param address Sector base address; must be aligned to sector size
 */
int Flash::eraseSector(const uintptr_t address) {
    if(kLogErase) {
        Logger::Notice("SectorErase(%06x)", address);
    }

    // validate address
    if(address & (this->info->sectorSizeBytes() - 1)) {
        return Error::UnalignedAddress;
    }

    // do erase
    return this->eraseWithAddress(this->info->cmdEraseSector, address,
            this->info->timeoutSectorErase);
}

/**
 * @brief Erase a block
 *
 * Erase a block starting at the given address.
 *
 * @param address Block base address; must be aligned to block size
 */
int Flash::eraseBlock(const uintptr_t address) {
    if(kLogErase) {
        Logger::Notice("BlockErase(%06x)", address);
    }

    // validate address
    if(address & (this->info->blockSizeBytes() - 1)) {
        return Error::UnalignedAddress;
    }

    // do erase
    return this->eraseWithAddress(this->info->cmdEraseBlock, address,
            this->info->timeoutBlockErase);
}

/**
 * @brief Erase the entire chip
 *
 * @remark This is a potentially (very) slow operation, during which time the calling task
 * will be blocked.
 */
int Flash::eraseChip() {
    int err;

    if(kLogErase) {
        Logger::Notice("ChipErase");
    }

    // enable writing
    err = this->writeEnable();
    if(err) {
        return err;
    }

    // send the erase command
    etl::array<uint8_t, 1> cmd{{
        this->info->cmdEraseChip
    }};
    err = ExecCmd(cmd);
    if(err) {
        return err;
    }

    // wait for the erase to complete
    return this->waitForCompletion(this->info->timeoutChipErase);
}

/**
 * @brief Perform a software reset on the flash.
 *
 * Most flash chips have a certain time interval after reset that must be observed before certain
 * types of accesses (program/erase operations) can be performed.
 */
int Flash::reset() {
    int err;

    // reset enable
    if(this->info->cmdResetEnable) {
        etl::array<uint8_t, 1> cmd{{
            this->info->cmdResetEnable
        }};
        err = ExecCmd(cmd);
        if(err) {
            return err;
        }
    }

    // do the reset
    etl::array<uint8_t, 1> cmd{{
        this->info->cmdReset
    }};
    return ExecCmd(cmd);
}



/**
 * @brief Execute a command and read payload
 *
 * Execute a command on the flash, then read one or more payload bytes.
 */
int Flash::ExecCmdReadBlocking(etl::span<const uint8_t> cmd, etl::span<uint8_t> data) {
    Ecode_t err;
    int ret{0};

    // assert /CS
    SetCsAsserted(true);

    // output command
    err = SPIDRV_MTransmitB(sl_spidrv_eusart_flash_handle, cmd.data(), cmd.size());
    if(err != ECODE_EMDRV_SPIDRV_OK) {
        ret = Error::IoCommand;
        goto done;
    }

    // receive payload (if any)
    if(data.size()) {
        err = SPIDRV_MReceiveB(sl_spidrv_eusart_flash_handle, data.data(), data.size());
        if(err != ECODE_EMDRV_SPIDRV_OK) {
            ret = Error::IoPayload;
            goto done;
        }
    }

done:;
    // de-assert CS
    SetCsAsserted(false);
    return ret;
}

/**
 * @brief Execute a command and write payload
 *
 * Execute a command on the flash, then write one or more payload bytes.
 */
int Flash::ExecCmdWriteBlocking(etl::span<const uint8_t> cmd, etl::span<const uint8_t> data) {
    Ecode_t err;
    int ret{0};

    // assert /CS
    SetCsAsserted(true);

    // output command
    err = SPIDRV_MTransmitB(sl_spidrv_eusart_flash_handle, cmd.data(), cmd.size());
    if(err != ECODE_EMDRV_SPIDRV_OK) {
        ret = Error::IoCommand;
        goto done;
    }

    // receive payload (if any)
    if(data.size()) {
        err = SPIDRV_MTransmitB(sl_spidrv_eusart_flash_handle, data.data(), data.size());
        if(err != ECODE_EMDRV_SPIDRV_OK) {
            ret = Error::IoPayload;
            goto done;
        }
    }

done:;
    // de-assert CS
    SetCsAsserted(false);
    return ret;
}
