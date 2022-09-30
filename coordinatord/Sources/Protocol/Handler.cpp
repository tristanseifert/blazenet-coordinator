#include <stdexcept>
#include <sys/time.h>

#include <fmt/format.h>
#include <event2/event.h>

#include "Support/Confd.h"
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
    // load configuration
    this->reloadConfig(false);

    // perform initialization
    this->initBeaconBuffer();
    this->uploadBeaconFrame(true);
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
 * @brief Load the protocol handler configuration
 *
 * This will set up stuff such as the beacon (interval, network UUID, supported features)
 *
 * @param upload Whether configuration is uploaded to the radio as well
 */
void Handler::reloadConfig(const bool upload) {
    // read the beacon config
    auto beaconInterval = Support::Confd::GetInteger("radio.beacon.interval").value_or(5000);
    if(beaconInterval < kMinBeaconInterval) {
        throw std::runtime_error(fmt::format("invalid beacon interval: {} (min {})",
                    beaconInterval, kMinBeaconInterval));
    }

    this->beaconInterval = std::chrono::milliseconds(beaconInterval);

    // upload config to radio if needed
    if(upload) {
        this->uploadBeaconFrame(true);
    }
}



/**
 * @brief Initialize the beacon frame
 *
 * This sets up the common part of the beacon frame that doesn't change.
 */
void Handler::initBeaconBuffer() {
    using namespace std::chrono_literals;

    // TODO: implement this properly
    this->beaconBuffer.resize(16);
    this->beaconBuffer[0] = std::byte{0x0f};

    const char *buf = "smoke weed 420";
    memcpy(this->beaconBuffer.data() + 1, buf, 14);
}

/**
 * @brief Upload beacon configuration to radio
 *
 * Send the beacon frame and current interval to the radio.
 *
 * @param frameChanged When set, the content of the beacon frame has changed and needs to be
 *        uploaded again
 */
void Handler::uploadBeaconFrame(const bool frameChanged) {
    if(frameChanged) {
        this->radio->setBeaconConfig(true, this->beaconInterval, this->beaconBuffer);
    } else {
        this->radio->setBeaconConfig(true, this->beaconInterval);
    }
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
    this->radio->queueTransmitPacket(Radio::PacketPriority::NetworkControl, this->beaconBuffer);
}
