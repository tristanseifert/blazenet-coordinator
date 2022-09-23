#ifndef RADIO_H
#define RADIO_H

#include <array>
#include <cstddef>
#include <string>

namespace Transports {
class TransportBase;

namespace Response {
struct GetInfo;
struct GetStatus;
}
}

/**
 * @brief Interface to a radio
 *
 * Provides an interface to a radio, attached to the system through an underlying transport. This
 * class encapsulates all of the logic in formatting commands.
 */
class Radio {
    private:
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

    private:
        void queryRadioInfo(Transports::Response::GetInfo &);
        void queryStatus(Transports::Response::GetStatus &);

    private:
        /// Interface used to communicate with the radio
        std::shared_ptr<Transports::TransportBase> transport;

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
