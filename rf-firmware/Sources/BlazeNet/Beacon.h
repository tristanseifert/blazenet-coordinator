#ifndef BLAZENET_BEACON_H
#define BLAZENET_BEACON_H

#include <stddef.h>
#include <stdint.h>
#include <etl/span.h>

#include "Packet/Handler.h"
#include "Rtos/Rtos.h"

namespace BlazeNet {
/**
 * @brief Beacon handler
 *
 * This handles autonomously transmitting the network beacon frames. The content of this frame is
 * set by the host at various points, as well as the general beacon configuration and whether this
 * feature is enabled.
 */
class Beacon {
    private:
        /// Maximum size of a beacon frame (bytes)
        constexpr static const size_t kMaxBeaconSize{192};

    public:
        static void Init();

        static void CommsRegained();
        static void CommsLost();

        static void SetEnabled(const bool isEnabled);
        static void SetInterval(const uintptr_t interval);
        static int SetPayload(etl::span<const uint8_t> payload);

    private:
        static void EmitBeacon();

    private:
        /// Is automatic beaconing enabled?
        static bool gEnabled;

        /// Packet buffer for the beacon frame
        static Packet::Handler::TxPacketBuffer *gPacket;
        /// Mutex to guard access to the storage/packet buffer
        static SemaphoreHandle_t gPacketLock;

        /// Periodic beaconing timer
        static TimerHandle_t gTimer;
};
}

#endif
