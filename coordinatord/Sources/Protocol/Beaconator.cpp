#include <cmath>
#include <sstream>
#include <stdexcept>
#include <sys/time.h>

#include <BlazeNet/Types.h>
#include <event2/event.h>
#include <fmt/format.h>

#include "Support/Confd.h"
#include "Support/EventLoop.h"
#include "Support/HexDump.h"
#include "Support/Logging.h"
#include "Radio.h"
#include "Handler.h"
#include "Beaconator.h"

using namespace Protocol;

/**
 * @brief Initialize the beacon manager
 *
 * Read the beacon configuration and formulate the initial beacon frame.
 *
 * @param handler Protocol handler that instantiated us
 */
Beaconator::Beaconator(Handler &handler) : handler(handler) {
    this->reloadConfig(false);

    this->updateBeaconBuffer();
    this->uploadBeaconFrame(true);
}

/**
 * @brief Disable beaconing on the radio
 *
 * This will inhibit automatic transmission of beacon frames, if it's enabled.
 */
Beaconator::~Beaconator() {
    this->handler.radio->setBeaconConfig(false, this->interval);
}



/**
 * @brief Load the protocol handler configuration
 *
 * This will set up stuff such as the beacon (interval, network UUID, supported features)
 *
 * @param upload Whether configuration is uploaded to the radio as well
 */
void Beaconator::reloadConfig(const bool upload) {
    // read the beacon interval
    auto beaconInterval = Support::Confd::GetInteger(kConfBeaconInterval).value_or(5000);
    if(beaconInterval < kMinBeaconInterval) {
        throw std::runtime_error(fmt::format("invalid beacon interval: {} (min {})",
                    beaconInterval, kMinBeaconInterval));
    }

    this->interval = std::chrono::milliseconds(
            static_cast<size_t>(std::ceil(beaconInterval / 10.) * 10));
    PLOG_DEBUG << "Beacon interval: " << this->interval.count() << " ms";

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
void Beaconator::updateBeaconBuffer() {
    using namespace std::chrono_literals;
    auto &radio = this->handler.radio;

    // figure out how big we need for the basic MAC and beacon header
    constexpr static const size_t kPayloadBaseBytes{
        sizeof(BlazeNet::Types::Mac::Header) + sizeof(BlazeNet::Types::Beacon::Header)
    };

    this->buffer.resize(sizeof(BlazeNet::Types::Phy::Header) + kPayloadBaseBytes);
    std::fill(this->buffer.begin(), this->buffer.end(), std::byte(0));

    // prepare PHY header
    auto phyHdr = reinterpret_cast<BlazeNet::Types::Phy::Header *>(this->buffer.data());

    // build MAC header
    auto macHdr = reinterpret_cast<BlazeNet::Types::Mac::Header *>(phyHdr->payload);

    macHdr->flags = BlazeNet::Types::Mac::HeaderFlags::EndpointNetControl;
    macHdr->sequence = 0;
    macHdr->source = radio->getAddress();
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
    if(this->buffer.size() > 0xff) {
        throw std::runtime_error(fmt::format("beacon too large: {}", this->buffer.size()));
    }
    this->buffer[0] = std::byte(this->buffer.size() - 1);

    // secrete the final frame (for debugging)
    if(kLogBeaconFrame) {
        std::stringstream str;
        Support::HexDump::dumpBuffer<std::stringstream, std::byte>(str, this->buffer);
        PLOG_DEBUG << "Beacon frame:" << std::endl << str.str();
    }
}

/**
 * @brief Upload beacon configuration to radio
 *
 * Send the beacon frame and current interval to the radio.
 *
 * @param frameChanged When set, the content of the beacon frame has changed and needs to be
 *        uploaded again
 */
void Beaconator::uploadBeaconFrame(const bool frameChanged) {
    auto &radio = this->handler.radio;

    if(frameChanged) {
        radio->setBeaconConfig(true, this->interval, this->buffer);
    } else {
        radio->setBeaconConfig(true, this->interval);
    }
}
