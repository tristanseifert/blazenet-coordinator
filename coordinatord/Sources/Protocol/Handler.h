#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

class Radio;

namespace Protocol {
/**
 * @brief Low level protocol packet handler
 *
 * This class implements the lower level (Layer 2) part of the BlazeNet protocol. It's responsible
 * for framing packets for transmission over the air, and extracting data from packets that have
 * been received.
 */
class Handler {
    private:
        /// Config key for beacon interval (in ms)
        constexpr static const std::string_view kConfBeaconInterval{"radio.beacon.interval"};
        /// Config key for network id
        constexpr static const std::string_view kConfBeaconId{"radio.beacon.id"};

    public:
        /**
         * @brief Minimum beacon interval (msec)
         */
        constexpr static const size_t kMinBeaconInterval{1'000};

    public:
        Handler(const std::shared_ptr<Radio> &radio);
        ~Handler();

        void reloadConfig(const bool upload);

    private:
        void updateBeaconBuffer();
        void uploadBeaconFrame(const bool frameChanged);

    private:
        /// Underlying radio we're communicating with
        std::shared_ptr<Radio> radio;

        /// Beacon interval
        std::chrono::milliseconds beaconInterval{0};
        /// Network identifier
        std::array<std::byte, 16> networkId;
        /// Buffer for beacon frames
        std::vector<std::byte> beaconBuffer;

        /// Is pairing of new devices over-the-air enabled?
        bool inBandPairingEnabled{false};
};
}

#endif
