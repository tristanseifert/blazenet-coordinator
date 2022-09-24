#ifndef HOSTIF_HANDLERS_IRQSTATUS_H
#define HOSTIF_HANDLERS_IRQSTATUS_H

#include <etl/span.h>

#include "HostIf/Commands.h"
#include "HostIf/IrqManager.h"
#include "Rtos/Rtos.h"

namespace HostIf::Handlers {
/**
 * @brief Process an "GetCounters" command
 *
 * Read out the performance counters and clears them.
 */
struct IrqStatus {
    /**
     * @brief Handle a read by the host
     *
     * This will get the currently pending interrupts. Pending interrupts _are_ affected by the
     * interrupt mask configured via the IrqConfig command.
     */
    static int DoRead(const uint8_t, const size_t requested, etl::span<uint8_t> outBuffer) {
        // ensure the entire packet is read
        if(outBuffer.size() < sizeof(Response::IrqStatus)) {
            return -1;
        }

        // get pending interrupts, and acknowledge them
        taskENTER_CRITICAL();
        const auto pending = IrqManager::GetPending();
        IrqManager::Acknowledge(pending);
        taskEXIT_CRITICAL();

        // build packet
        Response::IrqStatus temp{};

        temp.commandError = TestFlags(pending & Interrupt::CommandError);
        temp.rxQueueNotEmpty = TestFlags(pending & Interrupt::PacketReceived);
        temp.txPacket = TestFlags(pending & Interrupt::PacketTransmitted);
        temp.txQueueEmpty = TestFlags(pending & Interrupt::TxQueueEmpty);

        // secrete it
        const auto actualBytes = etl::min(requested, sizeof(temp));
        memcpy(outBuffer.data(), &temp, etl::min(actualBytes, outBuffer.size()));

        return actualBytes;
    }

    /**
     * @brief Handle a write from the host
     *
     * Acknowledges the interrupts specified by the host.
     */
    static int DoWrite(const uint8_t, etl::span<const uint8_t> payload) {
        // ensure we got the whole command
        if(payload.size() < sizeof(Request::IrqStatus)) {
            return -1;
        }

        auto req = reinterpret_cast<const Request::IrqStatus *>(payload.data());

        // build up irq acknowledge map
        Interrupt ack{Interrupt::None};

        if(req->commandError) {
            ack |= Interrupt::CommandError;
        }
        if(req->rxQueueNotEmpty) {
            ack |= Interrupt::PacketReceived;
        }
        if(req->txPacket) {
            ack |= Interrupt::PacketTransmitted;
        }
        if(req->txQueueEmpty) {
            ack |= Interrupt::TxQueueEmpty;
        }

        IrqManager::Acknowledge(ack);

        // success
        return 0;
    }
};
}

#endif
