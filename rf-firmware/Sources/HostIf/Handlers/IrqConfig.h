#ifndef HOSTIF_HANDLERS_IRQCONFIG_H
#define HOSTIF_HANDLERS_IRQCONFIG_H

#include <etl/span.h>

#include "HostIf/Commands.h"
#include "HostIf/IrqManager.h"

namespace HostIf::Handlers {
/**
 * @brief Process an "IrqConfig" command
 *
 * Supports updating the interrupt mask.
 */
struct IrqConfig {
    /**
     * @brief Handle a read by the host
     *
     * Returns the current interrupt mask status.
     */
    static int DoRead(const uint8_t, const size_t requested, etl::span<uint8_t> outBuffer) {
        // validate
        if(requested < sizeof(Response::IrqConfig)) {
            return -1;
        }

        auto res = reinterpret_cast<Response::IrqConfig *>(outBuffer.data());
        memset(res, 0, sizeof(*res));

        // fill in the enabled irq's
        auto mask = IrqManager::GetMask();

        res->commandError = TestFlags(mask & Interrupt::CommandError);
        res->rxQueueNotEmpty = TestFlags(mask & Interrupt::PacketReceived);
        res->txPacket = TestFlags(mask & Interrupt::PacketTransmitted);
        res->txQueueEmpty = TestFlags(mask & Interrupt::TxQueueEmpty);

        // success
        return sizeof(*res);
    }

    /**
     * @brief Handle a write from the host
     *
     * Update the interrupt mask according to the specified command.
     */
    static int DoWrite(const uint8_t, etl::span<const uint8_t> payload) {
        // validate payload
        if(payload.size() < sizeof(Request::IrqConfig)) {
            return -1;
        }

        auto req = reinterpret_cast<const Request::IrqConfig *>(payload.data());

        // build irq mask
        Interrupt newMask{Interrupt::None};

        if(req->commandError) {
            newMask |= Interrupt::CommandError;
        }
        if(req->rxQueueNotEmpty) {
            newMask |= Interrupt::PacketReceived;
        }
        if(req->txPacket) {
            newMask |= Interrupt::PacketTransmitted;
        }
        if(req->txQueueEmpty) {
            newMask |= Interrupt::TxQueueEmpty;
        }

        Logger::Notice("IrqConfig: mask=%08x", static_cast<uintptr_t>(newMask));

        // update it
        IrqManager::SetMask(newMask);
        return 0;
    }
};
}

#endif
