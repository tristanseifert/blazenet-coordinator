#include <cstring>
#include <stdexcept>
#include <fmt/format.h>

#include "Support/Logging.h"
#include "Transports/Base.h"
#include "Transports/Commands.h"

#include "Radio.h"

/**
 * @brief Initialize the radio handler
 *
 * Query some information from the radio device for quick retrieval later, and apply the initial
 * configuration.
 */
Radio::Radio(const std::shared_ptr<Transports::TransportBase> &_transport) : transport(_transport) {
    /*
     * Read out general information about the radio, to ensure that we can successfully
     * communicate with it, and store it for later.
     */
    Transports::Response::GetInfo info;
    this->queryRadioInfo(info);

    if(info.fw.protocolVersion != kProtocolVersion) {
        throw std::runtime_error(fmt::format("incompatible radio protocol version ${:02x}",
                    info.fw.protocolVersion));
    }

    memcpy(this->eui64.data(), info.hw.eui64, sizeof(info.hw.eui64));
    this->serial = std::string(info.hw.serial, strnlen(info.hw.serial, sizeof(info.hw.serial)));

    PLOG_INFO << "Radio s/n: " << this->serial << ", EUI64: " <<
        fmt::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", this->eui64[0],
                this->eui64[1], this->eui64[2], this->eui64[3], this->eui64[4], this->eui64[5],
                this->eui64[6], this->eui64[7]);

    this->maxTxPower = this->currentTxPower = info.radio.maxTxPower;

    /*
     * Then, configure the radio for operation by setting configuration that won't change for
     * the duration of its operation, such as the regulatory domain.
     *
     * No channel or beacon configuration will have been set yet: this is done later once we've
     * determined this configuration from other parts of the system.
     */
    // TODO: set regulatory domain
}

/**
 * @brief Clean up the radio handler
 *
 * Notify the radio that we're going away, then tear down all resources associated with the radio.
 */
Radio::~Radio() {

}



/**
 * @brief Synchronize radio configuration
 *
 * Set the radio configuration (such as channel, transmit power, etc.) to match the cached settings
 * we have stored.
 */
void Radio::uploadConfig() {
    // build the command
    Transports::Request::RadioConfig conf;

    conf.channel = this->currentChannel;
    conf.txPower = this->currentTxPower;

    // then submit it
    this->transport->sendCommandWithPayload(Transports::CommandId::RadioConfig,
            {reinterpret_cast<uint8_t *>(&conf), sizeof(conf)});

    // TODO: check for error

    this->isConfigDirty = false;
}



/**
 * @brief Execute the "get info" command
 *
 * @param outInfo Information structure to fill
 */
void Radio::queryRadioInfo(Transports::Response::GetInfo &outInfo) {
    this->transport->sendCommandWithResponse(Transports::CommandId::GetInfo,
            {reinterpret_cast<uint8_t *>(&outInfo), sizeof(outInfo)});

    if(outInfo.status != 1) {
        throw std::runtime_error(fmt::format("failed to get radio info: {}", outInfo.status));
    }
}

/**
 * @brief Execute the "get status" command
 *
 * @param outStatus Status register structure to fill
 */
void Radio::queryStatus(Transports::Response::GetStatus &outStatus) {
    this->transport->sendCommandWithResponse(Transports::CommandId::GetStatus,
            {reinterpret_cast<uint8_t *>(&outStatus), sizeof(outStatus)});
}
