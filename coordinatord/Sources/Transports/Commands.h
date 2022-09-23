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

    /**
     * @brief Get packet queue status
     *
     * Read out the current status of the internal packet queues, both the transmit and receive
     * queues.
     *
     * Reads are supported.
     */
    GetPacketQueueStatus                        = 0x05,

    /**
     * @brief Read packet
     *
     * Read the oldest packet out of the receive queue.
     *
     * Reads are supported.
     */
    ReadPacket                                  = 0x06,

    /**
     * @brief Transmit packet
     *
     * Queue a packet for transmission over the air.
     *
     * Writes are supported.
     */
    TransmitPacket                              = 0x07,
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

/**
 * @brief "Get packet queue status" command response
 *
 * Indicates the state of the receive and transmit queues.
 */
struct GetPacketQueueStatus {
    /// Is at least one receive packet pending?
    uint8_t rxPacketPending                     :1;
    /// Is there a transmit packet pending?
    uint8_t txPacketPending                     :1;
    uint8_t reserved                            :6;

    /// Size of the next packet to be read from the receive queue
    uint8_t rxPacketSize;
} __attribute__((packed));

/**
 * @brief "ReadPacket" command response
 *
 * Returns the contents of a buffer slot in the receive queue.
 *
 * @remark This does _not_ contain the packet payload length, as it's expected that you previously
 * retrieved this with a call to GetPacketQueueStatus.
 */
struct ReadPacket {
    /// Packet RSSI (in dB)
    int8_t rssi;
    /// Link quality (relative scale, where 0 is worst and 255 is best)
    uint8_t lqi;

    /// Actual payload data
    uint8_t payload[];
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

/**
 * @brief "TransmitPacket" command
 *
 * Enqueues a packet into the radio's transmit queue, which will cause it to be transmitted
 * immediately (if radio is available) or after any pending transmissions have completed.
 */
struct TransmitPacket {
    /**
     * @brief Packet priority value
     *
     * Indicates the relative priority of the packet, used for queuing the packet while it's
     * waiting for transmission.
     *
     * @remark Numerically _low_ values correspond to _low_ priorities, e.g. 0 is lowest.
     */
    uint8_t priority                            :2;
    uint8_t reserved                            :6;

    /// Packet payload data (including MAC headers)
    uint8_t data[];
} __attribute__((packed));
}
}

#endif
