#include <algorithm>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <event2/event.h>
#include <fmt/format.h>

#include "Config/Reader.h"
#include "Support/Confd.h"
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
    // start by resetting the radio and installing our irq handler
    this->transport->reset();

    this->transport->addIrqHandler([&](){
        this->irqHandler();
    });
    this->initWatchdog();

    // configure status polling, if configured
    auto pollInterval = Config::GetConfig().at_path("radio.general.pollInterval");
    if(pollInterval && pollInterval.is_number()) {
        const auto msec = pollInterval.value_or(0);
        if(msec) {
            this->initPolling(std::chrono::milliseconds(msec));
        }
    }

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
    this->fwVersion = std::string(info.fw.build, strnlen(info.fw.build, sizeof(info.fw.build)));

    PLOG_INFO << "Radio s/n: " << this->serial << ", EUI64: " <<
        fmt::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}", this->eui64[0],
                this->eui64[1], this->eui64[2], this->eui64[3], this->eui64[4], this->eui64[5],
                this->eui64[6], this->eui64[7]);

    this->maxTxPower = this->currentTxPower = info.radio.maxTxPower;

    /*
     * Do initial setup: configure interrupts and set up performance counter stuff
     */
    Transports::Request::IrqConfig irqConf{};
    irqConf.rxQueueNotEmpty = true;
    irqConf.txQueueEmpty = true;

    this->setIrqConfig(irqConf);

    this->initCounterReader();

    /*
     * Then, configure the radio for operation by setting configuration data. We read this out of
     * the runtime configuration settings land, the same as we would if we got a request to
     * refresh the radio config at runtime.
     */
    this->reloadConfig(true);
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

    if(this->irqWatchdog) {
        event_del(this->irqWatchdog);
        event_free(this->irqWatchdog);
    }

    if(this->pollTimer) {
        event_del(this->pollTimer);
        event_free(this->pollTimer);
    }
}



/**
 * @brief Read the radio configuration
 *
 * Use the runtime configuration mechanism to load the radio settings (such as channel, transmit
 * power, regulatory domain, etc.) and apply it to our internal configuration.
 *
 * @param apply When set, the configuration is uploaded to the radio
 */
void Radio::reloadConfig(const bool upload) {
    // channel
    auto channel = Support::Confd::GetInteger(kConfPhyChannel);
    if(!channel) {
        throw std::runtime_error("failed to read `radio.phy.channel`");
    }

    this->setChannel(*channel);

    // transmit power (convert float dBm -> deci-dBm)
    auto txPower = Support::Confd::GetReal(kConfPhyTxPower);
    if(!txPower) {
        throw std::runtime_error("failed to read `radio.phy.txPower`");
    }

    const size_t deciDbmTx = std::max(0., *txPower * 10.);
    this->setTxPower(deciDbmTx);

    PLOG_VERBOSE << "Read radio config: channel=" << *channel << ", tx power="
        << (deciDbmTx / 10.) << " dBm";

    // TODO: set regulatory domain

    // radio short address (from config file)
    auto item = Config::GetConfig().at_path("network.addresses.mine");
    if(!item || !item.is_number()) {
        throw std::runtime_error("invalid coordinator address (key `network.addresses.mine`)");
    }

    this->currentShortAddress = item.value_or(0);
    PLOG_DEBUG << "Coordinator address: " << fmt::format("${:04x}", this->currentShortAddress);

    // upload to radio if requested
    if(upload) {
        this->uploadConfig();
    }
}

/**
 * @brief Synchronize radio configuration
 *
 * Set the radio configuration (such as channel, transmit power, etc.) to match the cached settings
 * we have stored.
 */
void Radio::uploadConfig() {
    // build the command
    Transports::Request::RadioConfig conf{};

    conf.channel = this->currentChannel;
    conf.txPower = this->currentTxPower;
    conf.myAddress = this->currentShortAddress;

    // then submit it
    std::lock_guard lg(this->transportLock);
    this->transport->sendCommandWithPayload(Transports::CommandId::RadioConfig,
            {reinterpret_cast<std::byte *>(&conf), sizeof(conf)});

    // check that the packet was queued (error flag not set)
    this->ensureCmdSuccess("RadioConfig");
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
    this->ensureCmdSuccess("BeaconConfig");
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
    Transports::Response::GetCounters counters{};

    // read out the counters
    this->transport->sendCommandWithResponse(Transports::CommandId::GetCounters,
            {reinterpret_cast<std::byte *>(&counters), sizeof(counters)});

    // check for success
    this->ensureCmdSuccess("GetCounters");

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
 * @brief Initialize the radio status polling timer
 *
 * This timer fires periodically to poll the radio's status registers, same as if an interrupt had
 * fired. It's intended to support radios that don't (properly) implement interrupts.
 *
 * @param interval Polling interval, in msec
 */
void Radio::initPolling(const std::chrono::milliseconds interval) {
    auto evbase = Support::EventLoop::Current()->getEvBase();

    // set up the timer object
    this->pollTimer = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<Radio *>(ctx)->pollTimerFired();
    }, this);
    if(!this->pollTimer) {
        throw std::runtime_error("failed to allocate poll timer event");
    }

    // set the interval
    const auto usec{interval.count() * 1000};
    struct timeval tv{
        .tv_sec  = static_cast<time_t>(usec / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(usec % 1'000'000U),
    };

    evtimer_add(this->pollTimer, &tv);
    PLOG_DEBUG << "Radio poll interval: " << usec << " µS";
}

/**
 * @brief Radio poll timer expired
 *
 * Read the radio status register and act upon any pending events.
 */
void Radio::pollTimerFired() {
    Transports::Response::IrqStatus irq{};

    std::lock_guard lg(this->transportLock);
    this->getPendingInterrupts(irq);

    this->irqHandlerCommon(irq);
}



/**
 * @brief Interrupt watchdog
 *
 * This is a periodic timer that checks when the last interrupt was received from the radio, and if
 * it has been too long, manually invokes the handler. This guards against interrupt pulses getting
 * lost.
 */
void Radio::initWatchdog() {
    auto evbase = Support::EventLoop::Current()->getEvBase();

    this->irqWatchdog = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<Radio *>(ctx)->irqWatchdogFired();
    }, this);
    if(!this->irqWatchdog) {
        throw std::runtime_error("failed to allocate irq watchdog event");
    }

    // get interval from config
    size_t usec{kIrqWatchdogInterval * 1000};

    auto item = Config::GetConfig().at_path("radio.general.irqWatchdogInterval");
    if(item && item.is_number()) {
        usec = item.value_or(kIrqWatchdogInterval) * 1000;
    }

    PLOG_VERBOSE << "irq watchdog timeout: " << usec << " µS";

    // set the interval
    struct timeval tv{
        .tv_sec  = static_cast<time_t>(usec / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(usec % 1'000'000U),
    };

    evtimer_add(this->irqWatchdog, &tv);
}

/**
 * @brief Handle the irq watchdog firing
 *
 * If the last irq was long enough ago, manually query the irq status.
 */
void Radio::irqWatchdogFired() {
    Transports::Response::IrqStatus irq{};

    // ignore if we haven't had any irq's yet
    if(!this->irqCounter) {
        return;
    }

    const auto now = std::chrono::high_resolution_clock::now();
    double msec = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->lastIrq).count();

    if(msec > kIrqWatchdogThreshold) {
        std::lock_guard lg(this->transportLock);
        this->getPendingInterrupts(irq);

        if(*((uint8_t *) &irq)) {
            this->numLostIrqs++;

            if(kIrqWatchdogLogging) {
                PLOG_WARNING << fmt::format("Lost IRQ: 0b{:08b}", *((uint8_t *) &irq));
            }
        }

        this->irqHandlerCommon(irq);
    }
}

/**
 * @brief Interrupt handler
 *
 * This is invoked by the underlying transport when it detects that the controller has asserted its
 * interrupt line.
 */
void Radio::irqHandler() {
    Transports::Response::IrqStatus irq{};
    this->irqCounter++;

    // get the pending interrupts flag
    std::lock_guard lg(this->transportLock);

    this->getPendingInterrupts(irq);
    this->irqHandlerCommon(irq);
}

/**
 * @brief Interrupt handler core
 */
void Radio::irqHandlerCommon(const Transports::Response::IrqStatus &irq) {
    try {
        // process the interrupt sources
        if(irq.rxQueueNotEmpty) {
            bool keepReading{true};

            // read packets until there's no more
            while(keepReading) {
                this->readPacket(keepReading);
            }
        }
        if(irq.txQueueEmpty) {
            this->drainTxQueue();
        }
    } catch(const std::exception &e) {
        PLOG_FATAL << "Radio irq handler failed: " << e.what();
        // TODO: better handling of errors here (this will abort the program)
        throw;
    }

    // update irq timestamp
    this->lastIrq = std::chrono::high_resolution_clock::now();
}
#include <sstream>

/**
 * @brief Read a pending packet.
 *
 * Check the packet queue status, and read out a packet from the queue. The packet is then
 * submitted to the upper layer packet handler.
 *
 * @param outRead Set when a packet was actually read out
 */
void Radio::readPacket(bool &outRead) {
    Transports::Response::GetPacketQueueStatus status{};
    Transports::Response::ReadPacket packet{};
    std::vector<std::byte> packetData;

    // read packet queue status
    this->queryPacketQueueStatus(status);
    if(!status.rxPacketPending) {
        outRead = false;
        return;
    }

    // allocate packet buffer and read
    packetData.resize(status.rxPacketSize);
    this->readPacket(packet, packetData);
    outRead = true;
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
    // execute request
    this->transport->sendCommandWithPayload(Transports::CommandId::IrqConfig,
            {reinterpret_cast<const std::byte *>(&config), sizeof(config)});

    // check for success
    this->ensureCmdSuccess("IrqConfig");
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
    // prepare our receive buffer, then do request
    this->rxBuffer.resize(sizeof(outHeader) + payloadBuf.size());
    this->transport->sendCommandWithResponse(Transports::CommandId::ReadPacket, this->rxBuffer);

    // check for success
    this->ensureCmdSuccess("ReadPacket");

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
    // prepare the transmit buffer
    this->txBuffer.resize(sizeof(header) + payload.size());
    memcpy(this->txBuffer.data(), &header, sizeof(header));
    memcpy(this->txBuffer.data() + sizeof(header), payload.data(), payload.size());

    // perform request
    this->transport->sendCommandWithPayload(Transports::CommandId::TransmitPacket, this->txBuffer);

    // check that the packet was queued (error flag not set)
    this->ensureCmdSuccess("TransmitPacket");
}

/**
 * @brief Read the interrupt status register
 *
 * @param outIrqs Variable to receive the currently pending interrupts
 */
void Radio::getPendingInterrupts(Transports::Response::IrqStatus &outIrqs) {
    this->transport->sendCommandWithResponse(Transports::CommandId::IrqStatus,
            {reinterpret_cast<std::byte *>(&outIrqs), sizeof(outIrqs)});
    this->ensureCmdSuccess("Read IrqStatus");
}

/**
 * @brief Acknowledge pending interrupts
 *
 * @param irqs Interrupts to acknowledge
 */
void Radio::acknowledgeInterrupts(const Transports::Request::IrqStatus &irqs) {
    this->transport->sendCommandWithPayload(Transports::CommandId::IrqStatus,
            {reinterpret_cast<const std::byte *>(&irqs), sizeof(irqs)});
    this->ensureCmdSuccess("Write IrqStatus");
}



/**
 * @brief Read s tatus register and ensure last command succeeded
 *
 * If the command success flag is not set (e.g. the last command failed) we'll throw an exception.
 *
 * @param commandName Optional name of the command that was last executed
 */
void Radio::ensureCmdSuccess(const std::string_view commandName) {
    Transports::Response::GetStatus status{};

    this->queryStatus(status);
    if(!status.cmdSuccess) {
        throw std::runtime_error(fmt::format("command failed: {}", commandName));
    }
}
