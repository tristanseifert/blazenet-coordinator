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
class Beaconator;

/**
 * @brief Low level protocol packet handler
 *
 * This class implements the lower level (Layer 2) part of the BlazeNet protocol. It's responsible
 * for framing packets for transmission over the air, and extracting data from packets that have
 * been received.
 */
class Handler {
    friend class Beaconator;

    public:
        Handler(const std::shared_ptr<Radio> &radio);
        ~Handler();

    private:
        /// Underlying radio we're communicating with
        std::shared_ptr<Radio> radio;

        /// Beacon manager
        std::shared_ptr<Beaconator> beaconator;
};
}

#endif
