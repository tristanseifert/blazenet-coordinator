#ifndef HOSTIF_COMMANDS_H
#define HOSTIF_COMMANDS_H

#include <stddef.h>
#include <stdint.h>

namespace HostIf {
/**
 * @brief Command identifiers
 */
enum class CommandId: uint8_t {
    NoOp                                        = 0x00,
    GetInfo                                     = 0x01,
    RadioConfig                                 = 0x02,

    /// Total number of defined commands
    NumCommands,
};

/**
 * @brief Host command header structure
 *
 * A small, packed structure received from the host. It indicates the command id and the length of
 * the (optional) payload.
 */
struct CommandHeader {
    /**
     * @brief Command identifier
     *
     * Command IDs are 7 bits in length. The high bit of the command is used to indicate
     * that the host is _reading_ data from the controller, rather than the other way
     * around.
     *
     * @seeAlso CommandId
     */
    uint8_t command;
    /// Number of payload bytes following the command
    uint8_t payloadLength;
} __attribute__((packed));

/// Holds command payload structures (sent to host)
namespace Response {
/**
 * @brief Information sent as part of a "Get Info" command
 */
struct GetInfo {
    /// Hardware feature flags
    enum HwFeatures: uint8_t {
        /// Controller has dedicated, private storage
        PrivateStorage                          = (1 << 0),
    };

    /// Command status (1 = success)
    uint8_t status;

    /// Firmware version information
    struct {
        /// Protocol version (current is 1)
        uint8_t protocolVersion;
        /// Major version
        uint8_t major;
        /// Minor version
        uint8_t minor;

        /// Build revision (ASCII string)
        char build[8];
    } fw;

    /// Hardware information
    struct {
        /// Hardware revision
        uint8_t rev;
        /// Hardware features supported
        uint8_t features;

        /// Serial number (ASCII string)
        char serial[16];
        /// EUI-64 (for radio use)
        uint8_t eui64[8];
    } hw;

    /// Radio capabilities
    struct {
        /// Maximum transmit power (in 1/10th dBm)
        uint8_t maxTxPower;
    } radio;
} __attribute__((packed));
};


/**
 * @brief Packet formats for requests sent by the host to the controller
 */
namespace Request {
/**
 * @brief "RadioConfig" command request
 *
 * This command is used to configure the radio PHY on the device for proper operation.
 */
struct RadioConfig {
    /// Channel number to use
    uint16_t channel;
    /**
     * @brief Maximum transmit power (in ⅒th of dBm) for any outgoing packets
     *
     * This is the power level used for multicast and broadcast frames, as well as network
     * management frames such as beacons. Unicast communications may use a (continuously adjusted)
     * lower transmit power.
     */
    uint16_t txPower;
} __attribute__((packed));
};
}

#endif
