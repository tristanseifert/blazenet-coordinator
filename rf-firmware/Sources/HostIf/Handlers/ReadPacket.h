#ifndef HOSTIF_HANDLERS_READPACKET_H
#define HOSTIF_HANDLERS_READPACKET_H

#include <string.h>
#include <etl/span.h>

#include "HostIf/Commands.h"
#include "Packet/Handler.h"

namespace HostIf::Handlers {
/**
 * @brief Process a "ReadPacket" command
 *
 * Read out the topmost packet on the receive queue.
 */
struct ReadPacket {
    /**
     * @brief Handle a read by the host
     *
     * Secrete a packet, including bonus header information, out into the buffer.
     *
     * @TODO What happens if the packet is 254 or 255 bytes payload?
     */
    static int DoRead(const uint8_t, const size_t requested, etl::span<uint8_t> outBuffer) {
        Response::ReadPacket temp;

        // get the packet
        auto pbuf = Packet::Handler::PopRxQueue();

        // build the header of the struct
        const auto actualNum = etl::min(requested, sizeof(temp) + pbuf->packetSize);

        temp.rssi = pbuf->rssi;
        temp.lqi = pbuf->lqi;

        // copy header and payload
        memcpy(outBuffer.data(), &temp, etl::min(actualNum, sizeof(temp)));
        if(actualNum > sizeof(temp)) {
            memcpy(outBuffer.data() + sizeof(temp), pbuf->data, actualNum - sizeof(temp));
        }

        // release packet
        Packet::Handler::DiscardRxPacket(pbuf);

        return actualNum;
    }
};
}

#endif
