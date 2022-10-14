#include <cbor.h>
#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

#include "Radio.h"
#include "Support/Cbor.h"
#include "Support/Logging.h"

#include "Config.h"

using namespace Rpc::Endpoints;

/**
 * @brief Process the config request
 *
 * The payload should be a CBOR map, which hasa get" key, which is equal to the name of the
 * configuration item to work on.
 *
 * @remark Currently, the configuration is read-only. In the future, it may support writing; though
 *         most changeable config is stored in confd, which can easily be updated.
 */
void Config::Handle(ClientConnection *client, const struct cbor_item_t *payload) {
    if(auto get = Support::CborMapGet(payload, "get")) {
        if(cbor_isa_string(get)) {
            std::string key(reinterpret_cast<char *>(cbor_string_handle(get)));
            std::transform(key.begin(), key.end(), key.begin(),
                    [](unsigned char c){ return std::tolower(c); });

            PLOG_VERBOSE << "Config get: '" << key << "'";

            if(key == "radio") {
                // TODO: implement
            } else {
                throw std::runtime_error(fmt::format("unknown config key `{}`", key));
            }
        } else {
            throw std::runtime_error("invalid config request (expected string for `get`)");
        }
    }
    // neither a get nor a set request
    else {
        throw std::runtime_error("invalid config request (missing `get` key)");
    }
}
