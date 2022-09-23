#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include <memory>

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
    public:
        Handler(const std::shared_ptr<Radio> &radio);
        ~Handler();

    private:
        /// Underlying radio we're communicating with
        std::shared_ptr<Radio> radio;
};
}

#endif
