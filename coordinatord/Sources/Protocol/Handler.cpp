#include <cmath>
#include <stdexcept>
#include <sys/time.h>

#include <BlazeNet/Types.h>
#include <fmt/format.h>
#include <event2/event.h>

#include "Support/Confd.h"
#include "Support/EventLoop.h"
#include "Support/Logging.h"
#include "Radio.h"
#include "Beaconator.h"
#include "Handler.h"

using namespace Protocol;

/**
 * @brief Initialize the protocol packet handler
 *
 * @param radio Radio to communicate with (assumed to be set up already)
 */
Handler::Handler(const std::shared_ptr<Radio> &_radio) : radio(_radio) {
    // initialize sub-components
    this->beaconator = std::make_shared<Beaconator>(*this);
}

/**
 * @brief Clean up all resources
 */
Handler::~Handler() {
    // destroy child objects
    this->beaconator.reset();
}
