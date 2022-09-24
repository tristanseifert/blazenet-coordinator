#ifndef RADIO_H
#define RADIO_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <queue>

namespace Transports {
class TransportBase;

namespace Response {
struct GetInfo;
struct GetStatus;
struct GetPacketQueueStatus;
struct ReadPacket;
struct IrqConfig;
struct IrqStatus;
}

namespace Request {
using IrqConfig = Response::IrqConfig;
using IrqStatus = Response::IrqStatus;
struct TransmitPacket;
}
}

/**
 * @brief Interface to a radio
 *
 * Provides an interface to a radio, attached to the system through an underlying transport. This
 * class encapsulates all of the logic in formatting commands, and also owns a transmit queue
 * which holds packets until they're ready to be handled by the physical radio (with limited
 * buffer space.)
 */
class Radio {
    public:
        /**
         * @brief Packet priority values
         *
         * Several virtual queues exist in both the radio and our internal queue, which are divided
         * by the packet's priority.
         */
        enum class PacketPriority: uint8_t {
            /**
             * @brief Background/idle
             *
             * This is the lowest priority, for traffic that is not time critical. There is no
             * guarantee that packets in this queue are transmitted.
             */
            Background                          = 0x00,

            /**
             * @brief Standard
             *
             * Normal packets with no special requirements.
             */
            Normal                              = 0x01,

            /**
             * @brief Real time
             *
             * Packets that should be sent immediately, such as for controlling a device.
             */
            RealTime                            = 0x02,

            /**
             * @brief Network control
             *
             * For network control purposes, we really don't want to have the packets stalled, so
             * they get a super high priority queue.
             */
            NetworkControl                      = 0x03,

            /// Maximum valid priority level value
            MaxLevel                            = NetworkControl,
            /// Total number of priority levels
            NumLevels,
        };

        /**
         * @brief Transmit performance counters
         */
        struct TxCounters {
            /// Pckets discarded due to insufficient buffer space
            uint_least64_t bufferDiscards{0};
            /// Packets discarded due to allocation failures
            uint_least64_t allocDiscards{0};
            /// Packets discarded due to insufficient queue space
            uint_least64_t queueDiscards{0};

            /// Drops due to FIFO underruns
            uint_least64_t fifoDrops{0};
            /// Packets discarded because radio could not get clear channel
            uint_least64_t ccaFails{0};
            /// Number of successfully transmitted frames
            uint_least64_t goodFrames{0};

            /**
             * @brief Reset all counters
             */
            inline void reset() {
                this->bufferDiscards = this->allocDiscards = this->queueDiscards = 0;
                this->fifoDrops = this->ccaFails = this->goodFrames = 0;
            }
        };

        /**
         * @brief Receive performance counters
         */
        struct RxCounters {
            /// Packets discarded due to insufficient buffer space
            uint_least64_t bufferDiscards{0};
            /// Packets discarded due to allocation failures
            uint_least64_t allocDiscards{0};
            /// Packets discarded due to insufficient queue space
            uint_least64_t queueDiscards{0};

            /// FIFO overruns
            uint_least64_t fifoOverflows{0};
            /// Packets discarded due to framing errors
            uint_least64_t frameErrors{0};
            /// Number of successfully received frames
            uint_least64_t goodFrames{0};

            /**
             * @brief Reset all counters
             */
            inline void reset() {
                this->bufferDiscards = this->allocDiscards = this->queueDiscards = 0;
                this->fifoOverflows = this->frameErrors = this->goodFrames = 0;
            }
        };

    private:
        /**
         * @brief Structure representing a packet pending transmission
         */
        struct TxPacket {
            /// Transmission priority
            PacketPriority priority;

            /**
             * @brief Packet data
             *
             * This is the raw data to be transmitted in this packet. It must have all headers
             * already applied, including a PHY length counter in the first byte.
             */
            std::vector<std::byte> payload;
        };

        using TxQueue = std::queue<std::unique_ptr<TxPacket>>;

        /// Supported protocol version
        constexpr static const uint8_t kProtocolVersion{0x01};

        /// Minimum beacon interval (msec)
        constexpr static const size_t kMinBeaconInterval{1'000};
        /// Interrupt watchdog interval (msec)
        constexpr static const size_t kIrqWatchdogInterval{50};
        /// How long we can go without an irq (msec)
        constexpr static const size_t kIrqWatchdogThreshold{250};
        /// Performance counter read interval (sec)
        constexpr static const size_t kPerfCounterReadInterval{30};

    public:
        Radio(const std::shared_ptr<Transports::TransportBase> &transport);
        ~Radio();

        /**
         * @brief Update the radio channel
         *
         * Set the channel the radio will communicate on. This does not actually take effect
         * until a subsequent call to uploadConfig().
         *
         * @param newChannel Channel to change to
         */
        inline void setChannel(const uint16_t newChannel) {
            this->currentChannel = newChannel;
            this->isConfigDirty = true;
        }

        /**
         * @brief Update transmit power
         *
         * Set the power level to use for transmitting packets.
         *
         * @param newPower Power level in ⅒th of a dBm
         */
        inline void setTxPower(const uint16_t newPower) {
            this->currentTxPower = newPower;
            this->isConfigDirty = true;
        }

        void uploadConfig();

        void queueTransmitPacket(const PacketPriority priority,
                std::span<const std::byte> payload);

        /**
         * @brief Update the beacon configuration (without changing the packet)
         *
         * @param enabled Are beacon frames enabled?
         * @param interval Time interval between beacon frames
         */
        inline void setBeaconConfig(const bool enabled, const std::chrono::milliseconds interval) {
            this->setBeaconConfig(enabled, interval, {}, true);
        }
        /**
         * @brief Update the beacon configuration
         *
         * @param enabled Are beacon frames enabled?
         * @param interval Time interval between beacon frames
         * @param payload Contents of the beacon frame
         */
        inline void setBeaconConfig(const bool enabled, const std::chrono::milliseconds interval,
                std::span<const std::byte> payload) {
            this->setBeaconConfig(enabled, interval, payload, true);
        }
        /**
         * @brief Update the beacon frame contents
         *
         * Change the contents of the beacon frame without changing any other configuration.
         *
         * @param payload Contents of the beacon frame
         */
        inline void setBeaconConfig(std::span<const std::byte> payload) {
            using namespace std::chrono_literals;
            this->setBeaconConfig(false, 0ms, payload, false);
        }

        void resetCounters(const bool remote = false);

        /**
         * @brief Get receive performance counters
         */
        inline const auto &getRxCounters() const {
            return this->rxCounters;
        }
        /**
         * @brief Get transmit performance counters
         */
        inline const auto &getTxCounters() const {
            return this->txCounters;
        }

    private:
        void transmitPacket(const std::unique_ptr<TxPacket> &);
        void setBeaconConfig(const bool enabled, const std::chrono::milliseconds interval,
                std::span<const std::byte> payload, const bool updateConfig);

        void initCounterReader();
        void counterReaderFired();
        void queryCounters();

        void initWatchdog();
        void irqWatchdogFired();
        void irqHandler();
        void irqHandlerCommon(const Transports::Response::IrqStatus &);
        void readPacket(bool &);
        bool drainTxQueue();

        void queryRadioInfo(Transports::Response::GetInfo &);
        void queryStatus(Transports::Response::GetStatus &);
        void setIrqConfig(const Transports::Request::IrqConfig &);
        void queryPacketQueueStatus(Transports::Response::GetPacketQueueStatus &);
        void readPacket(Transports::Response::ReadPacket &, std::span<std::byte>);
        void transmitPacket(const Transports::Request::TransmitPacket &,
                std::span<const std::byte>);

        void getPendingInterrupts(Transports::Response::IrqStatus &);
        void acknowledgeInterrupts(const Transports::Request::IrqStatus &);

        void ensureCmdSuccess(const std::string_view);

    private:
        /// Interface used to communicate with the radio
        std::shared_ptr<Transports::TransportBase> transport;
        /// Lock guarding accesses to the radio
        std::mutex transportLock;

        /// Transmit packet queues
        std::array<TxQueue, 4> txQueues;
        /// Lock guarding accesses to the transmit queue
        std::mutex txQueueLock;
        /// Buffer used for transmitting packets
        std::vector<std::byte> txBuffer;
        /// Buffer used for receiving packets
        std::vector<std::byte> rxBuffer;

        /// EUI-64 address of the radio
        std::array<std::byte, 8> eui64;
        /// Serial number of the radio
        std::string serial;

        /// Is the radio configuration dirty?
        bool isConfigDirty{true};

        /// Channel number to communicate on
        uint16_t currentChannel{0xffff};

        /// Maximum supported transmit power (in ⅒th dBm)
        uint16_t maxTxPower;
        /// Current transmit power
        uint16_t currentTxPower;

        /// Number of interrupts triggered
        size_t irqCounter{0};
        /// Irq watchdog timer
        struct event *irqWatchdog{nullptr};
        /// last irq
        std::chrono::time_point<std::chrono::high_resolution_clock> lastIrq;

        /// Periodic event to read out the performance counters
        struct event *counterReader{nullptr};
        /// TX performance counters
        TxCounters txCounters{};
        /// RX performance counters
        RxCounters rxCounters{};
};

#endif
