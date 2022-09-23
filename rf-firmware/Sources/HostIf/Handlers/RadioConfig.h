#ifndef HOSTIF_HANDLERS_RADIOCONFIG_H
#define HOSTIF_HANDLERS_RADIOCONFIG_H

#include <string.h>
#include <etl/span.h>
#include <etl/utility.h>

#include "HostIf/Commands.h"
#include "Log/Logger.h"
#include "Radio/Task.h"

namespace HostIf::Handlers {
/**
 * @brief Process a "RadioConfig" command
 *
 * This will set the radio configuration.
 */
struct RadioConfig {
    /**
     * @brief Handle a write from the host
     *
     * This will update the radio configuration as requested.
     */
    static int DoWrite(const uint8_t, etl::span<const uint8_t> payload) {
        int err;

        // validate payload
        if(payload.size() < sizeof(Request::RadioConfig)) {
            return -1;
        }

        auto req = reinterpret_cast<const Request::RadioConfig *>(payload.data());

        // update channel
        err = Radio::Task::SetChannel(req->channel);
        if(err) {
            Logger::Warning("%s failed: %d", "RadioConfig set channel", err);
            return err;
        }

        // update TX power
        err = Radio::Task::SetTxPower(req->txPower);
        if(err) {
            Logger::Warning("%s failed: %d", "RadioConfig set power", err);
            return err;
        }

        Logger::Debug("RadioConfig: ch=%u, txpwr=%d", req->channel, req->txPower);

        return 0;
    }
};
}

#endif
