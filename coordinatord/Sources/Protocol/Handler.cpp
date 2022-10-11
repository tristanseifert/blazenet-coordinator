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
    this->updateBeaconBuffer();
    this->uploadBeaconFrame(true);
}

/**
 * @brief Clean up all resources
 */
Handler::~Handler() {
}



/**
 * @brief Load the protocol handler configuration
 *
 * This will set up stuff such as the beacon (interval, network UUID, supported features)
 *
 * @param upload Whether configuration is uploaded to the radio as well
 */
void Handler::reloadConfig(const bool upload) {
    // read the beacon interval
    auto beaconInterval = Support::Confd::GetInteger(kConfBeaconInterval).value_or(5000);
    if(beaconInterval < kMinBeaconInterval) {
        throw std::runtime_error(fmt::format("invalid beacon interval: {} (min {})",
                    beaconInterval, kMinBeaconInterval));
    }

    this->beaconInterval = std::chrono::milliseconds(
            static_cast<size_t>(std::ceil(beaconInterval / 10.) * 10));
    PLOG_VERBOSE << "Beacon interval: " << this->beaconInterval.count() << " ms";

    // read the network UUID
    const auto idBytesRead = Support::Confd::GetBlob(kConfBeaconId, this->networkId);
    if(idBytesRead != this->networkId.size()) {
        throw std::runtime_error(fmt::format("failed to read network id (`{}`): got {} bytes",
                    kConfBeaconId, idBytesRead));
    }

    // upload config to radio if needed
    if(upload) {
        this->uploadBeaconFrame(true);
    }
}



/**
 * @brief Generate the beacon frame
 *
 * Regenerate the beacon frame buffer.
 */
void Handler::updateBeaconBuffer() {
    using namespace std::chrono_literals;

    // figure out how big we need for the basic MAC and beacon header
    constexpr static const size_t kPayloadBaseBytes{
        sizeof(BlazeNet::Types::Mac::Header) + sizeof(BlazeNet::Types::Beacon::Header)
    };

    this->beaconBuffer.resize(sizeof(BlazeNet::Types::Phy::Header) + kPayloadBaseBytes);
    std::fill(this->beaconBuffer.begin(), this->beaconBuffer.end(), std::byte(0));

    // prepare PHY header
    auto phyHdr = reinterpret_cast<BlazeNet::Types::Phy::Header *>(this->beaconBuffer.data());

    // build MAC header (XXX: proper PHY header offset)
    auto macHdr = reinterpret_cast<BlazeNet::Types::Mac::Header *>(phyHdr->payload);

    macHdr->flags = BlazeNet::Types::Mac::HeaderFlags::EndpointNetControl;
    macHdr->sequence = 0;
    macHdr->source = this->radio->getAddress();
    macHdr->destination = BlazeNet::Types::Mac::kBroadcastAddress;

    // build beacon header
    auto beaconHdr = reinterpret_cast<BlazeNet::Types::Beacon::Header *>(phyHdr->payload
            + sizeof(*macHdr));
    beaconHdr->version = BlazeNet::Types::kProtocolVersion;

    if(this->inBandPairingEnabled) {
        beaconHdr->flags |= BlazeNet::Types::Beacon::HeaderFlags::PairingEnable;
    }

    memcpy(beaconHdr->id, this->networkId.data(), sizeof(beaconHdr->id));

    // TODO: bonus beacon headers (pending traffic map, etc.)

    // fill in PHY header with the final length
    if(this->beaconBuffer.size() > 0xff) {
        throw std::runtime_error(fmt::format("beacon too large: {}", this->beaconBuffer.size()));
    }
    this->beaconBuffer[0] = std::byte(this->beaconBuffer.size() - 1);
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
