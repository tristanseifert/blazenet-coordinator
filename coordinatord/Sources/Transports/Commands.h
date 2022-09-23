/**
 * @file
 * 
 * @brief Radio command structures
 *
 * This file contains the definition of command IDs, as well as the structures that define the
 * payload format for commands that have one.
 *
 * @remark All multibyte values are sent in little endian byte order
 */
#ifndef TRANSPORTS_COMMANDS_H
#define TRANSPORTS_COMMANDS_H

#include <stddef.h>
#include <stdint.h>

namespace Transports {
/**
 * @brief Command IDs
 *
 * This enum contains the command IDs for all currently supported commands.
 *
 * @remark Commands are 7-bit identifiers, as the high bit is reserved.
 */
enum class CommandId: uint8_t {
    /**
     * @brief No-op
     *
     * This command does nothing. Any payload data written is ignored.
     *
     * Writes are supported.
     */
    NoOp                                        = 0x00,

    /**
     * @brief Get controller information
     *
     * Read out the controller information descriptor. This defines the software version running
     * on the controller, as well as its hardware capabilities.
     *
     * Reads are supported.
     */
    GetInfo                                     = 0x01,

    /**
     * @brief Configure radio PHY
     *
     * Set the configuration for the radio air interface. This includes the current regulatory
     * region, frequency band, channel, transmit power, and so forth.
     *
     * Writes are supported.
     */
    RadioConfig                                 = 0x02,

    /**
     * @brief Get status register
     *
     * Read out the status register, which indicates various events that have occurred.
     *
     * Reads are supported.
     */
    GetStatus                                   = 0x03,
};

/**
 * @brief Command header structure
 *
 * @seeAlso CommandId
 */
struct CommandHeader {
    /// Command to execute (7 bits; high bit indicates r/w)
    uint8_t id;
    /// Length of payload (or requested length of response)
    uint8_t length;
} __attribute__((packed));

/**
 * @brief Packet formats for responses from the controller to the host
 */
namespace Response {
/**
 * @brief "Get Info" command response
 */
struct GetInfo {
    /// Hardware feature flags
    enum HwFeatures: uint8_t {
        /// Controller has dedicated, private storage
        PrivateStorage                          = (1 << 0),
    };

    /// Status (1 = success)
    uint8_t status;

    /// Firmware version information
    struct {
        /// Protocol version (current is 1)
        uint8_t protocolVersion;
        /// Major software version
        uint8_t major;
        /// Minor software version
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
        /// EUI-64 address (used for radio)
        uint8_t eui64[8];
    } hw;

    /// Radio capabilities
    struct {
        /// Maximum transmit power (in ⅒th dBm)
        uint8_t maxTxPower;
    } radio;
} __attribute__((packed));

/**
 * @brief "Get Status" command response
 *
 * This is basically one gigantic bitfield of event flags.
 */
struct GetStatus {
    /// Last command resulted in error
    uint8_t errorFlag                           :1;
    /// Radio is active (tuned to channel and receiving or transmitting)
    uint8_t radioActive                         :1;

    /// At least one packet is pending in the receive queue
    uint8_t rxQueueNotEmpty                     :1;
    /// The receive queue is full
    uint8_t rxQueueFull                         :1;
    /// Receive queue overflow (packets have been discarded)
    uint8_t rxQueueOverflow                     :1;

    /// Transmit queue is empty
    uint8_t txQueueEmpty                        :1;
    /// Transmit queue is full
    uint8_t txQueueFull                         :1;
    /// Transmit queue overflow (packets have been discarded)
    uint8_t txQueueOverflow                     :1;
} __attribute__((packed));
}

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
}
}

#endif
