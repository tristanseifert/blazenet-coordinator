#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include <cstdint>

namespace Rpc {
/**
 * @brief Current supported RPC version
 */
constexpr static const uint16_t kCurrentVersion{0x0100};

/**
 * @brief Header structure prepended to all RPC requests
 */
struct RequestHeader {
    /**
     * @brief RPC version
     *
     * Indicates the version of this structure and the underlying protocol
     *
     * @seeAlso kCurrentVersion
     */
    uint16_t version;

    /**
     * @brief Total request size, in bytes
     *
     * Indicates the size of the RPC request, _including_ this header.
     */
    uint16_t length;

    /**
     * @brief Message endpoint
     *
     * Indicates what internal codepath is used to handle this message.
     */
    uint8_t endpoint;

    /**
     * @brief Message tag
     *
     * An arbitrary integer value that can be used to correlate the requests and their responses
     * on the caller's side.
     */
    uint8_t tag;

    /**
     * @brief Payload data
     *
     * Any additional data indicated by the `length` field is considered to be the payload of the
     * message, which starts here.
     */
    uint8_t payload[];
} __attribute__((packed));

/**
 * @brief Request RPC endpoints
 */
enum RequestEndpoint: uint8_t {
    /**
     * @brief Configuration endpoint
     *
     * Allows reading out the running configuration of a variety of components.
     */
    Config                              = 0x01,

    /**
     * @brief Status endpoint
     *
     * Read out the current status of various components.
     */
    Status                              = 0x02,
};
}

#endif
