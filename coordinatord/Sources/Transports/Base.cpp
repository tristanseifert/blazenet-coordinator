#include <toml++/toml.h>

#include "Base.h"

#ifdef WITH_TRANSPORT_SPIDEV
#include "Transports/Spidev.h"
#endif

using namespace Transports;

/**
 * @brief Initialize a transport, given a configuration
 *
 * @param root TOML table containing the transport configuration (already validated)
 */
std::shared_ptr<TransportBase> TransportBase::Make(const toml::table &root) {
    // get the type string
    const std::string typeStr = root["type"].value_or("");

    // invoke initializer
#if WITH_TRANSPORT_SPIDEV
    if(typeStr == "spidev") {
        return std::make_shared<Transports::Spidev>(root);
    }
#endif
    // if we get here, no transport could be created
    return nullptr;
}
