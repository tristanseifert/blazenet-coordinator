#include <rail.h>
#include <stdlib.h>

#include "HostIf/IrqManager.h"
#include "Log/Logger.h"
#include "Handler.h"

using namespace Packet;

bool Handler::gRxOverflowFlag{false};
size_t Handler::gRxAllocBytes{0}, Handler::gRxQueueDiscarded{0}, Handler::gRxBufferDiscarded{0},
       Handler::gRxBufferAllocFailed{0};
Handler::RxQueueType *Handler::gRxQueue;

/**
 * @brief Initialize the packet handler
 */
void Handler::Init() {
    gRxQueue = new RxQueueType;
    REQUIRE(!!gRxQueue, "failed to allocate %s", "rx packet queue");
}

/**
 * @brief Enqueue a new packet into the receive buffer
 *
 * We'll allocate a buffer structure for this packet, copy the payload into it, then insert it at
 * the end of the receive queue. Interrupt and status flags are updated as well.
 *
 * @return Pointer to packet buffer, or `nullptr` if out of resources
 *
 * @TODO make this thread safe
 */
Handler::RxPacketBuffer *Handler::EnqueueRxPacket(const struct RAIL_RxPacketInfo &info,
        const struct RAIL_RxPacketDetails &details) {
    // ensure we've queue space
    if(gRxQueue->full()) {
        gRxOverflowFlag = true;
        gRxQueueDiscarded++;
        UpdateRxQueueState();
        return nullptr;
    }

    // calculate the size of the packet buffer structure, validate space vacancy and allocate
    const auto requiredBytes = sizeof(RxPacketBuffer) + info.packetBytes;

    if((gRxAllocBytes + requiredBytes) > kMaxRxBufferSize) {
        gRxOverflowFlag = true;
        gRxBufferDiscarded++;
        UpdateRxQueueState();
        return nullptr;
    }

    auto buffer = reinterpret_cast<RxPacketBuffer *>(malloc(requiredBytes));
    if(!buffer) {
        gRxOverflowFlag = true;
        gRxBufferAllocFailed++;
        UpdateRxQueueState();
        return nullptr;
    }

    // fill in the buffer
    buffer->packetSize = info.packetBytes;
    buffer->rssi = details.rssi;
    buffer->lqi = details.lqi;

    RAIL_CopyRxPacket(buffer->data, &info);

    // enqueue it
    gRxQueue->emplace(buffer);

    Logger::Trace("queue %u/%u (%p)", gRxQueue->available(), gRxQueue->capacity(), buffer);

    UpdateRxQueueState();

    return buffer;
}

/**
 * @brief Releases resources associated with this receive packet buffer
 */
void Handler::DiscardRxPacket(RxPacketBuffer *buffer) {
    const auto numBytes = sizeof(*buffer) + buffer->packetSize;

    free(buffer);
    gRxAllocBytes -= numBytes;
}

/**
 * @brief Update the state of the receive queue
 *
 * This updates the various flags and interrupts.
 */
void Handler::UpdateRxQueueState() {
    // update interrupt state
    if(!gRxQueue->empty()) {
        HostIf::IrqManager::Assert(HostIf::Interrupt::PacketReceived);
    } else {
        HostIf::IrqManager::Deassert(HostIf::Interrupt::PacketReceived);
    }
}
