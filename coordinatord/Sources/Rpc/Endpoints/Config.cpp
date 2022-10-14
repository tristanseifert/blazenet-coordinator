#include <cbor.h>
#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

#include "Radio.h"
#include "version.h"
#include "Rpc/ClientConnection.h"
#include "Rpc/Server.h"
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
void Config::Handle(ClientConnection *client, const cbor_item_t *payload) {
    if(auto get = Support::CborMapGet(payload, "get")) {
        if(cbor_isa_string(get)) {
            std::string key(reinterpret_cast<char *>(cbor_string_handle(get)));
            std::transform(key.begin(), key.end(), key.begin(),
                    [](unsigned char c){ return std::tolower(c); });

            if(key == "radio") {
                GetRadioCfg(client, payload);
            } else if(key == "version") {
                GetVersion(client, payload);
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



/**
 * @brief Read the radio configuration
 */
void Config::GetRadioCfg(ClientConnection *client, const cbor_item_t *) {
    // get the radio
    auto radio = client->getServer()->getRadio();
    if(!radio) {
        throw std::runtime_error("failed to get radio instance");
    }

    // build response
    auto root = cbor_new_definite_map(4);

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("txPower")),
        .value = cbor_move(cbor_build_float4(radio->getTxPower())),
    });
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("channel")),
        .value = cbor_move(cbor_build_uint32(radio->getChannel())),
    });
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("shortAddress")),
        .value = cbor_move(cbor_build_uint16(radio->getAddress())),
    });
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("sn")),
        .value = cbor_move(cbor_build_string(radio->getSerial().c_str())),
    });

    Reply(client, root);
}

/**
 * @brief Get the software version
 */
void Config::GetVersion(ClientConnection *client, const cbor_item_t *) {
    auto root = cbor_new_definite_map(2);

    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("version")),
        .value = cbor_move(cbor_build_string(kVersion)),
    });
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("build")),
        .value = cbor_move(cbor_build_string(kVersionGitHash)),
    });

    Reply(client, root);
}



/**
 * @brief Serialize CBOR item and send as reply
 */
void Config::Reply(ClientConnection *client, cbor_item_t* &root) {
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    try {
        client->reply({reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &) {
        free(rootBuf);
        throw;
    }
}
