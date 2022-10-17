#include <cbor.h>
#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

#include "Radio.h"
#include "Rpc/ClientConnection.h"
#include "Rpc/Server.h"
#include "Support/Cbor.h"
#include "Support/Logging.h"

#include "Status.h"

using namespace Rpc::Endpoints;

/**
 * @brief Process the status request
 *
 * The payload should be a CBOR map with a single key called `get` that contains the status item
 * that you wish to read:
 *
 * - radio.packet: Packet statistics (rx/tx performance counters)
 */
void Status::Handle(ClientConnection *client, const cbor_item_t *payload) {
    if(auto get = Support::CborMapGet(payload, "get")) {
        if(cbor_isa_string(get)) {
            std::string key(reinterpret_cast<char *>(cbor_string_handle(get)),
                    cbor_string_length(get));
            std::transform(key.begin(), key.end(), key.begin(),
                    [](unsigned char c){ return std::tolower(c); });

            if(key == "radio.counters") {
                GetRadioCounters(client, payload);
            } else {
                throw std::runtime_error(fmt::format("unknown status key `{}`", key));
            }
        } else {
            throw std::runtime_error("invalid status request (expected string for `get`)");
        }
    }
    else {
        throw std::runtime_error("invalid status request (missing `get` key)");
    }
}



/**
 * @brief Get radio packet status
 *
 * Read out the performance counters for the radio, and output relevant receive and transmit
 * counter values.
 */
void Status::GetRadioCounters(ClientConnection *client, const cbor_item_t *) {
    // get the radio
    auto radio = client->getServer()->getRadio();
    if(!radio) {
        throw std::runtime_error("failed to get radio instance");
    }

    // receive counters
    const auto &rxCounters = radio->getRxCounters();
    auto rxMap = cbor_new_definite_map(4);

    cbor_map_add(rxMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("good")),
        .value = cbor_move(cbor_build_uint64(rxCounters.goodFrames)),
    });
    cbor_map_add(rxMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("errors")),
        .value = cbor_move(cbor_build_uint64(rxCounters.frameErrors)),
    });
    cbor_map_add(rxMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("fifoOverflows")),
        .value = cbor_move(cbor_build_uint64(rxCounters.fifoOverflows)),
    });
    cbor_map_add(rxMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("queueDiscards")),
        .value = cbor_move(cbor_build_uint64(rxCounters.queueDiscards + rxCounters.allocDiscards
                    + rxCounters.bufferDiscards)),
    });

    // transmit counters
    const auto &txCounters = radio->getTxCounters();
    auto txMap = cbor_new_definite_map(4);

    cbor_map_add(txMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("good")),
        .value = cbor_move(cbor_build_uint64(txCounters.goodFrames)),
    });
    cbor_map_add(txMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("ccaFails")),
        .value = cbor_move(cbor_build_uint64(txCounters.ccaFails)),
    });
    cbor_map_add(txMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("fifoUnderruns")),
        .value = cbor_move(cbor_build_uint64(txCounters.fifoDrops)),
    });
    cbor_map_add(txMap, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("queueDiscards")),
        .value = cbor_move(cbor_build_uint64(txCounters.queueDiscards + txCounters.allocDiscards
                    + txCounters.bufferDiscards)),
    });

    // build response (root)
    auto root = cbor_new_definite_map(3);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("tx")),
        .value = cbor_move(txMap),
    });
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("rx")),
        .value = cbor_move(rxMap),
    });

    // TODO: timestamp the counters were last read
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("readAt")),
        .value = cbor_move(cbor_build_uint64(UINT64_MAX)),
    });

    client->reply(root);
}
