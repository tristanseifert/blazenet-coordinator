#ifndef PROTOCOL_BEACONATOR_H
#define PROTOCOL_BEACONATOR_H

#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace Protocol {
class Handler;

/**
 * @brief Network beacon manger
 *
 * This dude handles the beaconing configuration of an attached radio, including uploading a new
 * kind of beacon frame and formatting it based on the configuration.
 */
class Beaconator {
    private:
        /// Config key for beacon interval (in ms)
        constexpr static const std::string_view kConfBeaconInterval{"radio.beacon.interval"};
        /// Config key for network id
        constexpr static const std::string_view kConfBeaconId{"radio.beacon.id"};

        /**
         * @brief Minimum beacon interval (msec)
         */
        constexpr static const size_t kMinBeaconInterval{1'000};

        /// Whether beacon frame updates are logged
        constexpr static const bool kLogBeaconFrame{true};

    public:
        Beaconator(Handler &handler);
        ~Beaconator();

        void reloadConfig(const bool upload);

    private:
        void updateBeaconBuffer();
        void uploadBeaconFrame(const bool frameChanged);

    private:
        /// Handle to the protocol handler that owns us
        Handler &handler;

        /// Beacon interval
        std::chrono::milliseconds interval{0};
        /// Network identifier
        std::array<std::byte, 16> networkId{};

        /// Buffer for beacon frames
        std::vector<std::byte> buffer;

        /// Is pairing of new devices over-the-air enabled? (TODO: read from Handler)
        bool inBandPairingEnabled{false};
};
}

#endif
