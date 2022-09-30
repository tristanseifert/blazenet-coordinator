/**
 * @file
 *
 * @brief Provides runtime configuration via confd
 */
#ifndef CONFIG_RUNTIMECONFD_H
#define CONFIG_RUNTIMECONFD_H

#include <string>

#include <fmt/format.h>

#include "Radio.h"
#include "Support/Confd.h"
#include "Support/Logging.h"

namespace Config {
using Confd = Support::Confd;

/**
 * @brief Initialize runtime config support
 *
 * This opens the confd connection.
 */
inline static void InitRuntimeConfig() {
    Confd::Init();
}



/**
 * @brief Update radio configuration
 *
 * Apply the region setting, channel, and transmit power to the radio.
 */
inline static void UpdateRadioConfig(Radio *radio) {
    // channel
    auto channel = Confd::GetInteger("radio.phy.channel");
    if(!channel) {
        throw std::runtime_error("failed to read `radio.phy.channel`");
    }

    radio->setChannel(*channel);

    // transmit power (convert float dBm -> deci-dBm)
    auto txPower = Confd::GetReal("radio.phy.txPower");
    if(!txPower) {
        throw std::runtime_error("failed to read `radio.phy.txPower`");
    }

    const size_t deciDbmTx = std::max(0., *txPower * 10.);
    radio->setTxPower(deciDbmTx);

    PLOG_VERBOSE << "Read radio config: channel=" << *channel << ", tx power="
        << (deciDbmTx / 10.) << " dBm";
}
}

#endif
