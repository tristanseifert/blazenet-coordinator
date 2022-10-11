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
     * @brief Configure radio interrupts
     *
     * Allows updating which events generate an external interrupt toward the host.
     *
     * Writes are supported.
     */
    IrqConfig                                   = 0x04,

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

    /**
     * @brief Configure automatic beaconing
     *
     * Configure the controller's automatic beacon transmission feature. This sets up the interval
     * and contents of the beacon packet.
     *
     * Writes are supported.
     */
    BeaconConfig                                = 0x08,

    /**
     * @brief Get performance counters
     *
     * Get the current values of performance counters, and reset them. It's expected the host will
     * accumulate the counters with higher precision (64 bits) every time they are read.
     *
     * Reads are supported.
     */
    GetCounters                                 = 0x09,

    /**
     * @brief Interrupt status register
     *
     * Read the currently pending interrupts, and clear them.
     *
     * Reads and writes are supported.
     */
    IrqStatus                                   = 0x0A,
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
    /// Previous command executed successfully (updated after a command completes)
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

/**
 * @brief "GetCounters" command response
 *
 * Reads out various performance counters to the host. If this command completes successfully, the
 * counters will be cleared to zero.
 */
struct GetCounters {
    /// Current internal tick timestamp
    uint32_t currentTicks;

    /// Transmit queue
    struct {
        /// Current number of packets pending
        uint32_t packetsPending;
        /// Number of bytes currently allocated
        uint32_t bufferSize;

        /// Packets discarded because buffer size limit was reached
        uint32_t bufferDiscards;
        /// Packets discarded because allocation failed (other reason)
        uint32_t bufferAllocFails;
        /// Packets discarded because queue is full
        uint32_t queueDiscards;
    } txQueue;
    /// Radio (transmit)
    struct {
        /// Drops because FIFO is full
        uint32_t fifoDrops;
        /// CSMA detection fails
        uint32_t ccaFails;
        /// Number of successfully transmitted frames
        uint32_t goodFrames;
    } txRadio;

    /// Receive queue
    struct {
        /// Current number of packets pending
        uint32_t packetsPending;
        /// Number of bytes currently allocated
        uint32_t bufferSize;

        /// Packets discarded because buffer size limit was reached
        uint32_t bufferDiscards;
        /// Packets discarded because allocation failed (other reason)
        uint32_t bufferAllocFails;
        /// Packets discarded because queue is full
        uint32_t queueDiscards;
    } rxQueue;
    /// Radio (receive)
    struct {
        /// FIFO overflows
        uint32_t fifoOverflows;
        /// Frame errors
        uint32_t frameErrors;
        /// Number of good frames
        uint32_t goodFrames;
    } rxRadio;
} __attribute__((packed));

/**
 * @brief Response to an "IRQ Status" command
 *
 * This reads out the interrupt status register, showing which interrupts are currently active.
 */
struct IrqStatus {
    /**
     * @brief Command error interrupt
     *
     * Set: Asserted any time a radio command fails.
     */
    uint8_t commandError                        :1{0};

    /**
     * @brief Receive queue not empty
     *
     * Set: A packet was received and is waiting in the receive queue
     */
    uint8_t rxQueueNotEmpty                     :1{0};

    /**
     * @brief Packet transmitted
     *
     * Set: A packet was successfully transmitted
     */
    uint8_t txPacket                            :1{0};

    /**
     * @brief Transmit queue empty
     *
     * Set: The last pending packet has been transmitted
     */
    uint8_t txQueueEmpty                        :1{0};

    uint8_t reserved                            :4{};
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

    /**
     * @brief Radio short address
     *
     * 16-bit short address of the coordinator node; used for filtering of auto-ack messages and
     * when generating internal frames.
     */
    uint16_t myAddress;
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
using IrqConfig = Transports::Response::IrqConfig;

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

/**
 * @brief "IrqStatus" write command
 *
 * This is used to clear pending interrupts, and thus release the interrupt line state.
 */
using IrqStatus = Response::IrqStatus;
}
}

#endif
