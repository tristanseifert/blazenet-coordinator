#ifndef HOSTIF_HANDLERS_BEACONCONFIG_H
#define HOSTIF_HANDLERS_BEACONCONFIG_H

#include <etl/span.h>

#include "BlazeNet/Beacon.h"
#include "HostIf/Commands.h"

namespace HostIf::Handlers {
/**
 * @brief Process a "BeaconConfig" command
 *
 * Updates the configuration of the automatic beacon feature.
 */
struct BeaconConfig {
    /**
     * @brief Handle a write from the host
     *
     * Update the beacon configuration.
     */
    static int DoWrite(const uint8_t, etl::span<const uint8_t> payload) {
        int err{0};

        // ensure we have at least the first part of the header
        if(payload.size() < offsetof(Request::BeaconConfig, data)) {
            return -1;
        }

        auto req = reinterpret_cast<const Request::BeaconConfig *>(payload.data());
        const auto packetPayloadBytes = payload.size() - sizeof(*req);

        // update general variables
        if(req->updateConfig) {
            BlazeNet::Beacon::SetEnabled(!!req->enabled);
            BlazeNet::Beacon::SetInterval(req->interval);

            Logger::Notice("%s: %s, interval=%u ms", "BeaconConfig", req->enabled ? "on" : "off",
                    req->interval);
        }

        // update the packet payload if specified
        if(packetPayloadBytes) {
            auto packetPayload = payload.subspan<offsetof(Request::BeaconConfig, data)>();
            err = BlazeNet::Beacon::SetPayload(packetPayload);

            if(err) {
                Logger::Warning("%s failed: %u", "Beacon::SetPayload", err);
            } else {
                Logger::Notice("%s: payloadLength=%u", "BeaconConfig", packetPayload.size());
            }
        }

        return err;
    }
};
}

#endif
