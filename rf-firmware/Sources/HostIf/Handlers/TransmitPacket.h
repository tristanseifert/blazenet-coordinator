#ifndef HOSTIF_HANDLERS_TRANSMITPACKET_H
#define HOSTIF_HANDLERS_TRANSMITPACKET_H

#include <etl/span.h>

#include "HostIf/Commands.h"
#include "Packet/Handler.h"

namespace HostIf::Handlers {
/**
 * @brief Process a "TransmitPacket" command
 *
 * Takes the received packet, and inserts it into the radio's transmit queue.
 */
struct TransmitPacket {
    static int DoWrite(const uint8_t, etl::span<const uint8_t> payload) {
        using PacketPriority = Packet::Handler::TxPacketPriority;

        // validate payload
        if(payload.size() < sizeof(Request::TransmitPacket)) {
            return -1;
        }

        auto req = reinterpret_cast<const Request::TransmitPacket *>(payload.data());
        const auto packetPayloadBytes = payload.size() - sizeof(*req);

        // submit it for queuing
        PacketPriority pri;

        switch(req->priority) {
            case 0x00:
                pri = PacketPriority::Background;
                break;
            case 0x01:
                pri = PacketPriority::Normal;
                break;
            case 0x02:
                pri = PacketPriority::RealTime;
                break;
            case 0x03:
                pri = PacketPriority::NetworkControl;
                break;
        }

        auto packet = Packet::Handler::EneuqueTxPacket(pri, {req->data, packetPayloadBytes});
        return (packet == nullptr) ? -2 : 0;
    }
};
}

#endif
