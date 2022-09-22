#ifndef HOSTIF_HANDLERS_GETINFO_H
#define HOSTIF_HANDLERS_GETINFO_H

#include <string.h>
#include <etl/span.h>
#include <etl/utility.h>

#include "BuildInfo.h"
#include "HostIf/Commands.h"
#include "Hw/Identity.h"

namespace HostIf::Handlers {
/**
 * @brief Process a "GetInfo" request
 */
struct GetInfo {
    /**
     * @brief Read the info structure
     *
     * Populate an info structure in the provided buffer.
     */
    static int DoRead(const uint8_t, const size_t requested, etl::span<uint8_t> outBuffer) {
        // calculate the maximal size to reply with
        const auto toReply = etl::min(requested, sizeof(Response::GetInfo));
        auto info = reinterpret_cast<Response::GetInfo *>(outBuffer.data());

        info->status = 1;

        // software version
        info->fw.major = 0x00;
        info->fw.minor = 0x01;
        strncpy(info->fw.build, gBuildInfo.gitHash, sizeof(info->fw.build));

        // hardware information
        info->hw.rev = 1;
        info->hw.features = Response::GetInfo::HwFeatures::PrivateStorage;

        strncpy(info->hw.serial, Hw::Identity::GetSerial(), sizeof(info->hw.serial));
        memcpy(info->hw.eui64, Hw::Identity::GetEui64().data(), sizeof(info->hw.eui64));

        // radio info (TODO: get actual data)
        info->radio.maxTxPower = 140;

        return toReply;
    }
};
}

#endif
