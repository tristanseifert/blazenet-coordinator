#ifndef HOSTIF_HANDLERS_GETCOUNTERS_H
#define HOSTIF_HANDLERS_GETCOUNTERS_H

#include <etl/span.h>

#include "HostIf/Commands.h"
#include "Packet/Handler.h"
#include "Radio/Task.h"
#include "Rtos/Rtos.h"

namespace HostIf::Handlers {
/**
 * @brief Process an "GetCounters" command
 *
 * Read out the performance counters and clears them.
 */
struct GetCounters {
    /**
     * @brief Handle a read by the host
     *
     * Returns the current counter state, then clears it.
     */
    static int DoRead(const uint8_t, const size_t requested, etl::span<uint8_t> outBuffer) {
        // validate
        if(requested < sizeof(Response::GetCounters)) {
            return -1;
        }

        auto res = reinterpret_cast<Response::GetCounters *>(outBuffer.data());
        new (res) Response::GetCounters;

        // read out counters
        res->currentTicks = xTaskGetTickCount();

        Packet::Handler::ReadCounters(res);
        Radio::Task::ReadCounters(res);

        return requested;
    }
};
}

#endif
