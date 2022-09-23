#include <stdexcept>
#include <sys/time.h>
#include <fmt/format.h>
#include <event2/event.h>

#include "Support/EventLoop.h"
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
    // set up beacon frames
    this->initBeaconBuffer();
    this->initBeaconTimer();

}

/**
 * @brief Clean up all resources
 */
Handler::~Handler() {
    if(this->beaconTimerEvent) {
        event_del(this->beaconTimerEvent);
        event_free(this->beaconTimerEvent);
    }
}



/**
 * @brief Initialize the beacon frame
 *
 * This sets up the common part of the beacon frame that doesn't change.
 */
void Handler::initBeaconBuffer() {
    // TODO: implement this properly
    this->beaconBuffer.resize(16);
    this->beaconBuffer[0] = std::byte{0x0f};

    const char *buf = "smoke weed 420";
    memcpy(this->beaconBuffer.data() + 1, buf, 14);
}

/**
 * @brief Set up a periodic timer to trigger beacon frame transmissions
 */
void Handler::initBeaconTimer() {
    auto evbase = Support::EventLoop::Current()->getEvBase();
    this->beaconTimerEvent = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<Handler *>(ctx)->sendBeacon();
    }, this);
    if(!this->beaconTimerEvent) {
        throw std::runtime_error("failed to allocate beacon event");
    }

    // set the interval
    const size_t usec = 5'000'000;
    struct timeval tv{
        .tv_sec  = static_cast<time_t>(usec / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(usec % 1'000'000U),
    };

    evtimer_add(this->beaconTimerEvent, &tv);
}

/**
 * @brief Transmit a beacon frame.
 */
void Handler::sendBeacon() {
    PLOG_VERBOSE << "secreting beacon";
    this->radio->queueTransmitPacket(Radio::PacketPriority::NetworkControl, this->beaconBuffer);
}
