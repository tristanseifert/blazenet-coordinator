#include <algorithm>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <event2/event.h>
#include <fmt/format.h>

#include "Support/EventLoop.h"
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
     * Do initial setup: register an irq handler and reset the radio.
     */
    this->transport->reset();

    this->transport->addIrqHandler([&](){
        this->irqHandler();
    });

    Transports::Request::IrqConfig irqConf{};
    irqConf.rxQueueNotEmpty = true;
    irqConf.txQueueEmpty = true;

    this->setIrqConfig(irqConf);

    this->initCounterReader();

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
    // stop performance counter reading in the background
    if(this->counterReader) {
        event_del(this->counterReader);
        event_free(this->counterReader);
    }
}



/**
 * @brief Synchronize radio configuration
 *
 * Set the radio configuration (such as channel, transmit power, etc.) to match the cached settings
 * we have stored.
 */
void Radio::uploadConfig() {
    Transports::Response::GetStatus status;

    // build the command
    Transports::Request::RadioConfig conf{};

    conf.channel = this->currentChannel;
    conf.txPower = this->currentTxPower;

    // then submit it
    std::lock_guard lg(this->transportLock);
    this->transport->sendCommandWithPayload(Transports::CommandId::RadioConfig,
            {reinterpret_cast<std::byte *>(&conf), sizeof(conf)});

    // check that the packet was queued (error flag not set)
    this->queryStatus(status);
    if(!status.cmdSuccess) {
        throw std::runtime_error("failed to set radio config");
    }

    this->isConfigDirty = false;
}

/**
 * @brief Inscrete a packet for transmission
 *
 * Submit the specified packet for transmission.
 *
 * If there are no packets pending in the queue, the packet is written directly to the radio, but
 * otherwise it's just appended to the queue and when the next "tx complete" interrupt arrives,
 * will get sent.
 *
 * @param priority Priority level of the packet (for queuing)
 * @param payload Packet data to transmit (including PHY and MAC headers)
 */
void Radio::queueTransmitPacket(const PacketPriority priority,
        std::span<const std::byte> payload) {
    // if queue is empty, transmit the packet right away
    std::lock_guard lg(this->txQueueLock);
    if(std::all_of(this->txQueues.begin(), this->txQueues.end(),
                std::bind(&TxQueue::empty, std::placeholders::_1))) {
        // build the request header
        Transports::Request::TransmitPacket header{};
        header.priority = static_cast<uint8_t>(priority);

        // transmit to radio
        std::lock_guard lg(this->transportLock);

        try {
            this->transmitPacket(header, payload);

            // on success, we're done
            return;
        } catch(const std::exception &e) {
            PLOG_WARNING << "failed to direct tx packet: " << e.what();
        }
    }

    // otherwise (on error or stuff in queue,) queue the packet as normal for later
    auto pbuf = std::make_unique<TxPacket>();
    pbuf->priority = priority;
    pbuf->payload.resize(payload.size());
    std::copy(payload.begin(), payload.end(), pbuf->payload.begin());

    this->txQueues.at(static_cast<size_t>(priority)).emplace(std::move(pbuf));
}

/**
 * @brief Send the provided packet to the radio
 *
 * Transmit the packet to the radio for transmission over the air.
 *
 * @param packet Packet buffer previously queued
 */
void Radio::transmitPacket(const std::unique_ptr<TxPacket> &packet) {
    // build the request header
    Transports::Request::TransmitPacket header{};
    header.priority = static_cast<uint8_t>(packet->priority);

    // perform the command
    std::lock_guard lg(this->transportLock);
    this->transmitPacket(header, packet->payload);
}

/**
 * @brief Update the beacon configuration
 *
 * The radio is capable of autonomously transmitting beacon frames, with a high degree of timing
 * accuracy compared to the host. This configuration defines what data is part of this request.
 */
void Radio::setBeaconConfig(const bool enabled, const std::chrono::milliseconds interval,
        std::span<const std::byte> payload, const bool updateConfig) {
    Transports::Response::GetStatus status{};

    // validate inputs
    if(interval.count() < kMinBeaconInterval) {
        throw std::invalid_argument(fmt::format("interval too small (min {} msec)",
                    kMinBeaconInterval));
    } else if(interval.count() > UINT16_MAX) {
        throw std::invalid_argument(fmt::format("interval too large (max {} msec)", UINT16_MAX));
    }

    // prepare a buffer of the appropriate size
    std::vector<std::byte> buf;
    buf.resize(sizeof(Transports::Request::BeaconConfig) + payload.size());

    auto cmd = reinterpret_cast<Transports::Request::BeaconConfig *>(buf.data());
    new(cmd) Transports::Request::BeaconConfig;

    // fill in the structure
    cmd->updateConfig = updateConfig;

    if(updateConfig) {
        cmd->enabled = enabled;
        cmd->interval = interval.count();
    }

    if(!payload.empty()) {
        memcpy(cmd->data, payload.data(), payload.size());
    }

    // transmit the command
    this->transport->sendCommandWithPayload(Transports::CommandId::BeaconConfig, buf);

    // check for success
    this->queryStatus(status);
    if(!status.cmdSuccess) {
        throw std::runtime_error("radio reported error from updating beacon config");
    }
}



/**
 * @brief Reset performance counters
 *
 * Clear the local copies of the performance counters, and perform a dummy read of the counters
 * from the radio to clear them as well.
 *
 * @param remote When set, the radio's counters are cleared as well
 */
void Radio::resetCounters(const bool remote) {
    // clear the radio's counters by reading them out, if requested
    if(remote) {
        std::lock_guard lg(this->transportLock);
        this->queryCounters();
    }

    // clear local counter values
    this->rxCounters.reset();
    this->txCounters.reset();
}

/**
 * @brief Initialize the performance counter reading timer
 *
 * This will periodically query the device's performance counters in the background, to keep them
 * from overflowing.
 *
 * @seeAlso counterReaderFired
 */
void Radio::initCounterReader() {
    auto evbase = Support::EventLoop::Current()->getEvBase();
    this->counterReader = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<Radio *>(ctx)->counterReaderFired();
    }, this);
    if(!this->counterReader) {
        throw std::runtime_error("failed to allocate performance counter reading event");
    }

    // set the interval
    struct timeval tv{
        .tv_sec  = static_cast<time_t>(kPerfCounterReadInterval),
        .tv_usec = static_cast<suseconds_t>(0),
    };

    evtimer_add(this->counterReader, &tv);
}

/**
 * @brief Handle background reading of performance counters
 *
 * Triggered by a periodic timer to read the performance counters.
 *
 * @seeAlso initCounterReader
 */
void Radio::counterReaderFired() {
    {
        std::lock_guard lg(this->transportLock);
        this->queryCounters();
    }

    PLOG_VERBOSE << fmt::format("rx: fifo={},frame={} ok={}; queue buf={},alloc={},queue={}",
            this->rxCounters.fifoOverflows, this->rxCounters.frameErrors,
            this->rxCounters.goodFrames, this->rxCounters.bufferDiscards,
            this->rxCounters.allocDiscards, this->rxCounters.queueDiscards);
    PLOG_VERBOSE << fmt::format("tx: fifo={},csma={} ok={}; queue buf={},alloc={},queue={}",
            this->txCounters.fifoDrops, this->txCounters.ccaFails, this->txCounters.goodFrames,
            this->txCounters.bufferDiscards, this->txCounters.allocDiscards,
            this->txCounters.queueDiscards);
}

/**
 * @brief Read performance counters
 *
 * Query the radio for the current values of its performance counters. These are reset after the
 * read is completed.
 */
void Radio::queryCounters() {
    Transports::Response::GetStatus status{};
    Transports::Response::GetCounters counters{};

    // read out the counters
    this->transport->sendCommandWithResponse(Transports::CommandId::GetCounters,
            {reinterpret_cast<std::byte *>(&counters), sizeof(counters)});

    // check for success
    this->queryStatus(status);
    if(!status.cmdSuccess) {
        throw std::runtime_error("radio reported error reading performance counters");
    }

    // process transmit counters
    PLOG_VERBOSE << fmt::format("tx: pending={}, alloc={} bytes", counters.txQueue.packetsPending,
            counters.txQueue.bufferSize);

    this->txCounters.bufferDiscards += counters.txQueue.bufferDiscards;
    this->txCounters.allocDiscards += counters.txQueue.bufferAllocFails;
    this->txCounters.queueDiscards += counters.txQueue.queueDiscards;

    this->txCounters.fifoDrops += counters.txRadio.fifoDrops;
    this->txCounters.ccaFails += counters.txRadio.ccaFails;
    this->txCounters.goodFrames += counters.txRadio.goodFrames;

    // process receive counters
    PLOG_VERBOSE << fmt::format("rx: pending={}, alloc={} bytes", counters.rxQueue.packetsPending,
            counters.rxQueue.bufferSize);

    this->rxCounters.bufferDiscards += counters.rxQueue.bufferDiscards;
    this->rxCounters.allocDiscards += counters.rxQueue.bufferAllocFails;
    this->rxCounters.queueDiscards += counters.rxQueue.queueDiscards;

    this->rxCounters.fifoOverflows += counters.rxRadio.fifoOverflows;
    this->rxCounters.frameErrors += counters.rxRadio.frameErrors;
    this->rxCounters.goodFrames += counters.rxRadio.goodFrames;
}



/**
 * @brief Interrupt handler
 *
 * This is invoked by the underlying transport when it detects that the controller has asserted its
 * interrupt line.
 */
void Radio::irqHandler() {
    bool checkAgain;
    Transports::Response::GetStatus status{};

    std::lock_guard lg(this->transportLock);

    // read status until we've serviced everything that needs servicing
    do {
        this->queryStatus(status);
        checkAgain = false;

        PLOG_DEBUG << fmt::format("status register: 0b{:08b}", *((uint8_t *) &status));

        // read a packet
        if(status.rxQueueNotEmpty) {
            this->readPacket();
            checkAgain = true;
        }
        if(status.rxQueueOverflow) {
            // TODO: reset overflow flag
            // checkAgain = true;
        }

        // transmit queue is empty
        if(status.txQueueEmpty) {
            checkAgain = this->drainTxQueue();
        }
    } while(checkAgain);
}

/**
 * @brief Read a pending packet.
 *
 * Check the packet queue status, and read out a packet from the queue. The packet is then
 * submitted to the upper layer packet handler.
 */
void Radio::readPacket() {
    Transports::Response::GetPacketQueueStatus status{};
    Transports::Response::ReadPacket packet{};
    std::vector<std::byte> packetData;

    // read packet queue status
    this->queryPacketQueueStatus(status);
    if(!status.rxPacketPending) {
        throw std::runtime_error("no rx packet pending!");
    }

    PLOG_VERBOSE << fmt::format("rx packet pending: {} bytes", status.rxPacketSize);

    // allocate packet buffer and read
    packetData.resize(status.rxPacketSize);
    this->readPacket(packet, packetData);
}

/**
 * @brief Read packets out of our internal queue until the radio says "no more"
 *
 * This will write packets to the radio until a transmit command fails, or all buffered packets
 * have been dealt with.
 *
 * @return Whether any packets were sent to the radio
 */
bool Radio::drainTxQueue() {
    bool sent{false};
    std::lock_guard lg(this->txQueueLock);

    for(size_t i = 0; i < this->txQueues.size(); i++) {
        auto &queue = this->txQueues.at(3 - i);

        // skip empty queues
        if(queue.empty()) {
            continue;
        }

        // pop a packet buffer to read
        auto &packet = queue.front();
        try {
            this->transmitPacket(packet);
            sent = true;
        }
        // if this fails, abort the drainage process
        catch(const std::exception &e) {
            PLOG_WARNING << "failed to transmit packet during tx queue drain: " << e.what();
            return sent;
        }

        // it was sent to the radio, so we can remove it out of the queue
        queue.pop();
    }

    return sent;
}



/**
 * @brief Execute the "get info" command
 *
 * @param outInfo Information structure to fill
 */
void Radio::queryRadioInfo(Transports::Response::GetInfo &outInfo) {
    this->transport->sendCommandWithResponse(Transports::CommandId::GetInfo,
            {reinterpret_cast<std::byte *>(&outInfo), sizeof(outInfo)});

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
            {reinterpret_cast<std::byte *>(&outStatus), sizeof(outStatus)});
}

/**
 * @brief Set the radio's interrupt configuration
 *
 * @param config Interrupt configuration to apply
 */
void Radio::setIrqConfig(const Transports::Request::IrqConfig &config) {
    Transports::Response::GetStatus status{};

    // execute request
    this->transport->sendCommandWithPayload(Transports::CommandId::IrqConfig,
            {reinterpret_cast<const std::byte *>(&config), sizeof(config)});

    // check for success
    this->queryStatus(status);
    if(!status.cmdSuccess) {
        throw std::runtime_error("radio reported error setting irq config");
    }
}

/**
 * @brief Execute the "get packet queue status" command
 *
 * @param outStatus Packet queue status structure to fill
 */
void Radio::queryPacketQueueStatus(Transports::Response::GetPacketQueueStatus &outStatus) {
    this->transport->sendCommandWithResponse(Transports::CommandId::GetPacketQueueStatus,
            {reinterpret_cast<std::byte *>(&outStatus), sizeof(outStatus)});
}

/**
 * @brief Receive a packet from the radio
 *
 * The entire packet (plus header) is received into a temporary buffer, from which it's then
 * separated out into the two components.
 */
void Radio::readPacket(Transports::Response::ReadPacket &outHeader,
        std::span<std::byte> payloadBuf) {
    Transports::Response::GetStatus status{};

    // prepare our receive buffer, then do request
    this->rxBuffer.resize(sizeof(outHeader) + payloadBuf.size());
    this->transport->sendCommandWithResponse(Transports::CommandId::ReadPacket, this->rxBuffer);

    // check for success
    this->queryStatus(status);
    if(!status.cmdSuccess) {
        throw std::runtime_error("radio reported error from reading packet");
    }

    // copy out data
    auto header = reinterpret_cast<const Transports::Response::ReadPacket*>(this->rxBuffer.data());
    outHeader = *header;

    std::copy(this->rxBuffer.begin() + offsetof(Transports::Response::ReadPacket, payload),
            this->rxBuffer.end(), payloadBuf.begin());
}

/**
 * @brief Send a packet to the radio
 *
 * The packet will be queued for transmission in the radio's internal buffer, and then secreted
 * on to the air… eventually.
 *
 * @param header General information about the packet to transmit
 * @param payload Packet payload (including PHY and MAC headers)
 */
void Radio::transmitPacket(const Transports::Request::TransmitPacket &header,
        std::span<const std::byte> payload) {
    Transports::Response::GetStatus status{};

    // prepare the transmit buffer
    this->txBuffer.resize(sizeof(header) + payload.size());
    memcpy(this->txBuffer.data(), &header, sizeof(header));
    memcpy(this->txBuffer.data() + sizeof(header), payload.data(), payload.size());

    // perform request
    this->transport->sendCommandWithPayload(Transports::CommandId::TransmitPacket, this->txBuffer);

    // check that the packet was queued (error flag not set)
    this->queryStatus(status);
    if(!status.cmdSuccess) {
        throw std::runtime_error("radio reported error on queuing packet");
    }
}
