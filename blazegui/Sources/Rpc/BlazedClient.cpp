#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <cbor.h>
#include <event2/event.h>
#include <fmt/format.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <sstream>
#include <system_error>

#include "Config/Reader.h"
#include "Support/Cbor.h"
#include "Support/EventLoop.h"
#include "Support/HexDump.h"
#include "Support/Logging.h"

#include "BlazedClient.h"

using namespace Rpc;

/// Global shared instance of the blazed rpc client
static std::shared_ptr<BlazedClient> gClient;

/**
 * @brief Get the shared blazed RPC client
 *
 * If not already done, initialize the RPC client.
 */
std::shared_ptr<BlazedClient> &BlazedClient::The() {
    if(gClient) {
        return gClient;
    }

    gClient = std::make_shared<BlazedClient>();

    return gClient;
}

/**
 * @brief Clean up the blazed rpc client
 */
void BlazedClient::CleanUp() {
    gClient.reset();
}



/**
 * @brief Allocate the blazed client
 *
 * Read out the config for the RPC socket path
 */
BlazedClient::BlazedClient() {
    // TODO: read from config
    this->socketPath = "/var/run/blazed/rpc.sock";

    // set up buffers
    this->rxBuffer.reserve(kMaxPacketSize);
}

/**
 * @brief Clean up client resources
 *
 * This will close the RPC socket, if it's still open.
 */
BlazedClient::~BlazedClient() {
    this->tearDown();
}

/**
 * @brief Connect the RPC socket
 */
void BlazedClient::connect() {
    int fd, err;

    // close previous socket, if any
    if(this->socket != -1) {
        close(this->socket);
        this->socket = -1;
    }

    // create the socket
    fd = ::socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(fd == -1) {
        throw std::system_error(errno, std::generic_category(), "create rpc socket");
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, this->socketPath.native().c_str(), sizeof(addr.sun_path) - 1);

    // dial it
    err = ::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if(err == -1) {
        close(fd);
        throw std::system_error(errno, std::generic_category(), "dial rpc socket");
    }

    this->socket = fd;
}

/**
 * @brief Tear down the RPC socket
 *
 * Clean up all resources associated with the RPC socket to prepare it for an eventual
 * reconnection.
 */
void BlazedClient::tearDown() {
    if(this->socket != -1) {
        close(this->socket);
        this->socket = -1;
    }
}

/**
 * @brief Ensure the RPC connection is valid
 *
 * Establish the RPC connection if not already done.
 */
void BlazedClient::ensureConnection() {
    // connection never created?
    if(this->socket == -1) {
        return this->connect();
    }

    // TODO: ensure the socket is valid
}

/**
 * @brief Send a raw packet to the remote
 *
 * This assumes the packet already has a `struct RpcHeader` prepended.
 */
void BlazedClient::sendRaw(std::span<const std::byte> payload) {
    int err = write(this->socket, payload.data(), payload.size());
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write");
    }
}


/**
 * @brief Send a packet to the remote, adding a packet header
 *
 * Generate a full packet (including packet header) and send it to the remote.
 *
 * @return Tag value associated with the packet
 */
uint8_t BlazedClient::sendPacket(const uint8_t endpoint, std::span<const std::byte> payload) {
    std::vector<std::byte> buffer;
    buffer.resize(sizeof(struct RequestHeader) + payload.size(), std::byte{0});

    // build up the header
    auto hdr = reinterpret_cast<struct RequestHeader *>(buffer.data());
    hdr->version = kCurrentVersion;
    hdr->length = sizeof(*hdr) + payload.size();
    hdr->endpoint = endpoint;

    do {
        hdr->tag = ++this->nextTag;
        // TODO: ensure there isn't a packet with this tag outstanding
    } while(!hdr->tag);

    // copy payload
    if(!payload.empty()) {
        std::copy(payload.begin(), payload.end(), buffer.begin() + sizeof(*hdr));
    }

    // send and return tag
    this->sendRaw(buffer);
    return hdr->tag;
}

/**
 * @brief Serialize a CBOR message and send it
 *
 * @param endpoint Endpoint to submit the packet to
 * @param root CBOR item to serialize as the root of the message
 *
 * @return Tag of the transmitted packet
 */
uint8_t BlazedClient::sendPacket(const uint8_t endpoint, cbor_item_t* &root) {
    uint8_t tag;

    // serialize message
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    // then try to send the packet
    try {
        tag = this->sendPacket(endpoint, {reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &e) {
        free(rootBuf);
        this->tearDown();
        throw;
    }

    return tag;
}

/**
 * @brief Wait to receive a response
 *
 * Read the next full packet from the socket. Its tag must match the specified tag. On completion,
 * the full response is in the class receive buffer.
 *
 * @param expectedTag Tag we expect for the next message
 *
 * @return Payload of the message decoded as CBOR, if any
 */
cbor_item_t *BlazedClient::readResponse(const uint8_t expectedTag) {
    // wait to receive a message
    this->rxBuffer.resize(kMaxPacketSize);
    const auto read = recv(this->socket, this->rxBuffer.data(), this->rxBuffer.size(), 0);
    if(read == -1) {
        throw std::system_error(errno, std::generic_category(), "receive rpc response");
    }

    this->rxBuffer.resize(read);

    // validate the header
    if(this->rxBuffer.size() < sizeof(RequestHeader)) {
        throw std::runtime_error(fmt::format("packet too small (got {} bytes)", read));
    }

    auto hdr = reinterpret_cast<const RequestHeader *>(this->rxBuffer.data());

    if(hdr->version != kCurrentVersion) {
        throw std::runtime_error(fmt::format("invalid rpc version: ${:04x}", hdr->version));
    } else if(hdr->length < sizeof(*hdr) || hdr->length > this->rxBuffer.size()) {
        throw std::runtime_error(fmt::format("invalid header size ({}, have {})", hdr->length,
                    this->rxBuffer.size()));
    } else if(hdr->tag != expectedTag) {
        throw std::runtime_error(fmt::format("invalid tag: got ${:02x}, expected ${:02x}",
                    hdr->tag, expectedTag));
    }

    std::span<const std::byte> payload(reinterpret_cast<const std::byte *>(hdr->payload),
            hdr->length - sizeof(*hdr));

    // if there is a payload, decode it as CBOR
    cbor_item_t *cborItem{nullptr};

    if(!payload.empty()) {
        struct cbor_load_result result{};
        cborItem = cbor_load(reinterpret_cast<const cbor_data>(payload.data()), payload.size(),
                &result);

        if(result.error.code != CBOR_ERR_NONE) {
            throw std::runtime_error(fmt::format("cbor_load failed: {} (at ${:x})",
                        result.error.code, result.error.position));
        }
    }

    return cborItem;
}



/**
 * @brief Get remote software version
 *
 * Read out the version of the blazed daemon, as well as the radio firmware, if any.
 *
 * @param outVersion blazed version
 * @param outBuild blazed build (commit hash)
 * @param outRadioVersion Radio firmware version
 */
void BlazedClient::getVersion(std::string &outVersion, std::string &outBuild,
        std::string &outRadioVersion) {
    this->ensureConnection();

    // submit request and receive the response
    auto root = cbor_new_definite_map(1);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("get")),
        .value = cbor_move(cbor_build_string("version"))
    });

    auto response = this->sendWithResponse(RequestEndpoint::Config, root);

    // decode response
    if(auto version = Support::CborMapGet(response, "version")) {
        if(cbor_isa_string(version)) {
            outVersion = std::string(reinterpret_cast<const char *>(cbor_string_handle(version)));
        }
    }

    if(auto build = Support::CborMapGet(response, "build")) {
        if(cbor_isa_string(build)) {
            outBuild = std::string(reinterpret_cast<const char *>(cbor_string_handle(build)));
        }
    }

    if(auto version = Support::CborMapGet(response, "radioVersion")) {
        if(cbor_isa_string(version)) {
            outRadioVersion = std::string(
                    reinterpret_cast<const char *>(cbor_string_handle(version)));
        }
    }

    cbor_decref(&response);
}

/**
 * @brief Get current radio configuration
 *
 * @param outRegion Regulatory region the radio is operating in
 * @param outChannel Channel number the radio is using
 * @param outTxPower Transmit power (in dBm)
 */
void BlazedClient::getRadioConfig(std::string &outRegion, size_t &outChannel, double &outTxPower) {
    this->ensureConnection();

    // submit request and receive the response
    auto root = cbor_new_definite_map(1);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("get")),
        .value = cbor_move(cbor_build_string("radio"))
    });

    auto response = this->sendWithResponse(RequestEndpoint::Config, root);

    // decode response
    if(auto channel = Support::CborMapGet(response, "channel")) {
        if(cbor_isa_uint(channel)) {
            outChannel = Support::CborReadUint(channel);
        }
    }

    if(auto txPower = Support::CborMapGet(response, "txPower")) {
        if(cbor_isa_float_ctrl(txPower)) {
            outTxPower = Support::CborReadFloat(txPower);
        }
    }

    // TODO: region string

    cbor_decref(&response);
}

/**
 * @brief Get radio statistics
 *
 * This reads out relevant performance counters of the radio.
 *
 * @param outRxGood Total number of successfully received frames
 * @param outRxCorrupt Number of received frames with uncorrectable errors
 * @param outRxFifoOverruns Receive packets dropped due to FIFO overruns
 * @param outTxGood Total number of successfully transmitted frames (including beacons)
 * @param outTxCcaFails Number of frames that failed transmission due to carriers sense failure
 * @param outTxFifoUnderruns Transmit ackets dropped due to FIFO underruns
 */
void BlazedClient::getRadioStats(size_t &outRxGood, size_t &outRxCorrupt, size_t &outRxFifoOverruns,
        size_t &outTxGood, size_t &outTxCcaFails, size_t &outTxFifoUnderruns) {
    this->ensureConnection();
    std::stringstream hexDump;

    // submit request and receive response
    auto root = cbor_new_definite_map(1);
    cbor_map_add(root, (struct cbor_pair) {
        .key = cbor_move(cbor_build_string("get")),
        .value = cbor_move(cbor_build_string("radio.counters"))
    });

    auto response = this->sendWithResponse(RequestEndpoint::Status, root);

    // decode response
    const cbor_item_t *count{nullptr};

    auto rxCounters = Support::CborMapGet(response, "rx");
    if(rxCounters && cbor_isa_map(rxCounters)) {
        count = Support::CborMapGet(rxCounters, "good");
        if(count && cbor_isa_uint(count)) {
            outRxGood = Support::CborReadUint(count);
        }
        count = Support::CborMapGet(rxCounters, "errors");
        if(count && cbor_isa_uint(count)) {
            outRxCorrupt = Support::CborReadUint(count);
        }
        count = Support::CborMapGet(rxCounters, "fifoOverruns");
        if(count && cbor_isa_uint(count)) {
            outRxFifoOverruns = Support::CborReadUint(count);
        }
    } else {
        Support::HexDump::dumpBuffer<std::stringstream, std::byte>(hexDump, this->rxBuffer);
        PLOG_WARNING << "invalid rx counters field: " << hexDump.str();
    }

    auto txCounters = Support::CborMapGet(response, "tx");
    if(txCounters && cbor_isa_map(txCounters)) {
        count = Support::CborMapGet(txCounters, "good");
        if(count && cbor_isa_uint(count)) {
            outTxGood = Support::CborReadUint(count);
        }
        count = Support::CborMapGet(txCounters, "ccaFails");
        if(count && cbor_isa_uint(count)) {
            outTxCcaFails = Support::CborReadUint(count);
        }
        count = Support::CborMapGet(txCounters, "fifoUnderruns");
        if(count && cbor_isa_uint(count)) {
            outTxFifoUnderruns = Support::CborReadUint(count);
        }
    } else {
        Support::HexDump::dumpBuffer<std::stringstream, std::byte>(hexDump, this->rxBuffer);
        PLOG_WARNING << "invalid tx counters field: " << hexDump.str();
    }

    // clean up
    cbor_decref(&response);
}

/**
 * @brief Get client statistics
 *
 * Retrieve some statistics about associated clients.
 *
 * @param outNumConnected Total number of currently connected clients
 */
void BlazedClient::getClientStats(size_t &outNumConnected) {
    this->ensureConnection();
    // TODO: implement
}
