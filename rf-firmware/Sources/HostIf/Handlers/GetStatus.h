#ifndef HOSTIF_HANDLERS_GETSTATUS_H
#define HOSTIF_HANDLERS_GETSTATUS_H

#include <string.h>
#include <etl/span.h>

#include "HostIf/Commands.h"
#include "HostIf/IrqManager.h"
#include "HostIf/Task.h"
#include "Packet/Handler.h"
#include "Radio/Task.h"

namespace HostIf::Handlers {
/**
 * @brief Process a "GetStatus" command
 *
 * Allows reading of a virtual "status register" which indicates various flags pertaining to the
 * operation of the controller.
 */
struct GetStatus {
    /**
     * @brief Handle a read by the host
     *
     * Produce the flags in the status register
     */
    static int DoRead(const uint8_t, const size_t requested, etl::span<uint8_t> outBuffer) {
        Response::GetStatus temp;

        // error flag
        temp.errorFlag = Task::gErrorFlag;
        Task::gErrorFlag = false;

        // radio active
        temp.radioActive = Radio::Task::IsActive();

        // receive queue flags
        temp.rxQueueNotEmpty = !Packet::Handler::GetRxEmptyFlag();
        temp.rxQueueFull = Packet::Handler::GetRxFullFlag();
        temp.rxQueueOverflow = Packet::Handler::GetRxOverflowFlag();

        // transmit queue flags
        temp.txQueueEmpty = Packet::Handler::GetTxEmptyFlag();
        temp.txQueueOverflow = Packet::Handler::GetTxOverflowFlag();

        // clear interrupt flags
        IrqManager::Deassert(Interrupt::StatusReadCleared);

        // copy out the correct amount
        const auto actualNum = etl::min(requested, sizeof(temp));
        memcpy(outBuffer.data(), &temp, actualNum);
        return actualNum;
    }
};
}

#endif
