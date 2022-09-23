#ifndef RADIO_H
#define RADIO_H

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
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
}

namespace Request {
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

    private:
        void irqHandler();
        void readPacket();

        void queryRadioInfo(Transports::Response::GetInfo &);
        void queryStatus(Transports::Response::GetStatus &);
        void queryPacketQueueStatus(Transports::Response::GetPacketQueueStatus &);
        void readPacket(Transports::Response::ReadPacket &, std::span<std::byte>);
        void transmitPacket(const Transports::Request::TransmitPacket &,
                std::span<const std::byte>);

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
};

#endif
