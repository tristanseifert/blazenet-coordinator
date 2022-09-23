#include <stdexcept>
#include <fmt/format.h>
#include <event2/event.h>

#include "Support/Logging.h"
#include "Radio.h"
#include "Handler.h"

using namespace Protocol;

/**
 * @brief Initialize the protocol packet handler
 *
 * @param radio Radio to communicate with (assumed to be set up already)
 */
Handler::Handler(const std::shared_ptr<Radio> &_radio) : radio(_radio) {

}

/**
 * @brief Clean up all resources
 */
Handler::~Handler() {
    // TODO: do stuff here
}
