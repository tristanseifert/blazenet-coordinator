#ifndef HOSTIF_HANDLERS_GETPACKETQUEUESTATUS_H
#define HOSTIF_HANDLERS_GETPACKETQUEUESTATUS_H

#include <string.h>
#include <etl/span.h>

#include "HostIf/Commands.h"
#include "Packet/Handler.h"

namespace HostIf::Handlers {
/**
 * @brief Process a "GetPacketQueueStatus" command
 *
 * Returns the state of the receive and transmit queues.
 */
struct GetPacketQueueStatus {
    /**
     * @brief Handle a read by the host
     *
     * Produce the packet queue status flags.
     */
    static int DoRead(const uint8_t, const size_t requested, etl::span<uint8_t> outBuffer) {
        Response::GetPacketQueueStatus temp;

        // receive queue state
        auto rxPending = Packet::Handler::PeekRxQueue();
        temp.rxPacketPending = rxPending && !Packet::Handler::GetRxEmptyFlag();

        if(rxPending) {
            temp.rxPacketSize = rxPending->packetSize;
        }

        // TODO: transmit queue state

        // copy out the correct amount
        const auto actualNum = etl::min(requested, sizeof(temp));
        memcpy(outBuffer.data(), &temp, actualNum);
        return actualNum;
    }
};
}

#endif
