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
    GetStatus                                   = 0x03,
    IrqConfig                                   = 0x04,
    GetPacketQueueStatus                        = 0x05,
    ReadPacket                                  = 0x06,
    TransmitPacket                              = 0x07,
    BeaconConfig                                = 0x08,

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

/**
 * @brief "Get Status" command response
 *
 * This is basically one gigantic bitfield of event flags.
 */
struct GetStatus {
    /// Last command was successful
    uint8_t cmdSuccess                          :1;
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
 * @brief "Irq configuration" command response
 *
 * Indicates the state of which interrupts are masked (0) or allowed (1) to generate a physical
 * interrupt.
 */
struct IrqConfig {
    /**
     * @brief Command error interrupt
     *
     * Set: Asserted any time a radio command fails.
     *
     * Clear: Read status register
     */
    uint8_t commandError                        :1;

    /**
     * @brief Receive queue not empty
     *
     * Set: A packet was received and is waiting in the receive queue
     *
     * Clear: Read out all pending packets
     */
    uint8_t rxQueueNotEmpty                     :1;

    /**
     * @brief Packet transmitted
     *
     * Set: A packet was successfully transmitted
     *
     * Clear: Read status register
     */
    uint8_t txPacket                            :1;

    /**
     * @brief Transmit queue empty
     *
     * Set: The last pending packet has been transmitted
     *
     * Clear: Read status register
     */
    uint8_t txQueueEmpty                        :1;

    uint8_t reserved                            :4;
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

/**
 * @brief "IrqConfig" command
 *
 * This is the same format as the read out command.
 */
using IrqConfig = Response::IrqConfig;

/**
 * @brief "BeaconConfig" command
 *
 * Configures automatic beacon transmission. If only the first two fields are specified (that is,
 * the command is sent such that the payload length is 0) the payload will not be altered.
 */
struct BeaconConfig {
    /**
     * @brief Update configuration
     *
     * When set, the basic beacon configuration is updated. This can be cleared to only update the
     * packet payload instead of the configuration.
     */
    uint8_t updateConfig                        :1{0};

    /**
     * @brief Are automatic beacons enabled?
     */
    uint8_t enabled                             :1{0};

    uint8_t reserved                            :6{0};

    /**
     * @brief Beacon interval, in ms
     */
    uint16_t interval;

    /**
     * @brief Beacon frame payload
     *
     * All data that follows here is considered part of the beacon frame.
     */
    uint8_t data[];
} __attribute__((packed));
};
}

#endif
