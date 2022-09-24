#include <rail.h>

#include "Hw/Indicators.h"
#include "Log/Logger.h"
#include "Packet/Handler.h"
#include "Rtos/Rtos.h"

#include "Task.h"

using namespace Radio;

TaskHandle_t Task::gTask;
RAIL_Handle_t Task::gRail;

size_t Task::gRxFifoOverflows{0}, Task::gRxFrameErrors{0}, Task::gRxFrames{0};

uint16_t Task::gTxChannel{UINT16_MAX};
size_t Task::gTxFifoDrops{0}, Task::gTxCcaFails{0}, Task::gTxFrames{0};
Packet::Handler::TxPacketBuffer *Task::gLastTx{nullptr};

const RAIL_CsmaConfig_t Task::gCsmaConfig{
    // [0, 7] backoffs on 1st attempt
    .csmaMinBoExp       = 3,
    // [0, 31] backoffs for 3rd+ attempt
    .csmaMaxBoExp       = 5,
    // 5 total attempts (4 retries)
    .csmaTries          = 5,
    // clear channel threshold
    .ccaThreshold       = -75,
    // backoff duration: 50 symbols at 4µs/symbol
    .ccaBackoff         = 160,
    // listening period: 10 symbols at 4µS/symbol
    .ccaDuration        = 40,
    // total timeout for CSMA (µS)
    .csmaTimeout        = 5'000,
};

/**
 * @brief Initialize the radio task
 *
 * @param rail Radio library handle to use
 */
void Task::Init(RAIL_Handle_t rail) {
    gRail = rail;

    // create the task itself
    static StaticTask_t gStorage;
    static StackType_t gStack[kStackSize];

    gTask = xTaskCreateStatic([](auto param) {
        Task::Main();
    }, kName.data(), kStackSize, nullptr, kPriority, gStack, &gStorage);
    REQUIRE(!!gTask, "failed to initialize %s", "radio task");
}

/**
 * @brief Configure the automatic acknowledgement
 *
 * Sets up the automatic acknowledgement feature of the radio stack.
 */
void Task::InitAutoAck() {
    RAIL_AutoAckConfig_t autoAckConfig = {
        .enable = true,
        // wait up to 1ms for ack
        .ackTimeout = 1000,
        // "error" param ignored
        .rxTransitions = { RAIL_RF_STATE_RX, RAIL_RF_STATE_RX},
        // "error" param ignored
        .txTransitions = { RAIL_RF_STATE_RX, RAIL_RF_STATE_RX}
    };
    RAIL_Status_t err = RAIL_ConfigAutoAck(gRail, &autoAckConfig);

    REQUIRE(err == RAIL_STATUS_NO_ERROR, "%s failed: %d", "RAIL_ConfigAutoAck", err);
}



/**
 * @brief Task main loop
 */
void Task::Main() {
    int err;
    BaseType_t ok;

    // perform deferred setup
    Logger::Trace("%s: init", "Radio");

    RAIL_ResetFifo(gRail, true, true);

    // TODO: handle more stuff
    // InitAutoAck();

    // wait for event
    while(true) {
        uint32_t note;

        ok = xTaskNotifyWaitIndexed(kNotificationIndex, 0, NotifyBits::All, &note,
                portMAX_DELAY);
        REQUIRE(ok == pdTRUE, "%s failed: %d", "xTaskNotifyWaitIndexed", ok);

        // copy out a received packet
        if(note & NotifyBits::PacketReceived) {
            Hw::Indicators::PulseRx();

            // TODO: loop for all complete packets in FIFO
            ReadPacket();
        }
        // packet just finished transmitting
        if(note & NotifyBits::PacketTransmitted) {
            Hw::Indicators::PulseTx();

            HandleTxComplete();
        }
        // failed to transmit packet: channel busy. retry again
        if(note & NotifyBits::TxChannelBusy) {
            REQUIRE(gLastTx, "CSMA failed, but no current packet?");

            // ensure it's not over the attempts
            if(++gLastTx->csmaFailCount < kMaxCsmaFails) {
                if(kLogTxCsmaRetries) {
                    Logger::Notice("tx %p: CSMA retry %u/%u", gLastTx, gLastTx->csmaFailCount,
                            kMaxCsmaFails);
                }

                err = TxPacketImmediate(gLastTx);
                REQUIRE(!err, "%s failed: %d", "TxPacketImmediate", err);
            }
            // otherwise, discard the packet
            else {
                Logger::Warning("dropped packet %p due to CSMA fail", gLastTx);
                HandleTxComplete();
            }
        }
    }
}

/**
 * @brief Read a packet out of the receive FIFO
 *
 * Read the oldest pending packet out of the radio FIFO and deposit it into the packet handler
 * queue for processing later.
 */
void Task::ReadPacket() {
    RAIL_RxPacketInfo_t info;
    RAIL_RxPacketDetails_t details;

    // get packet handle and then acquire packet details
    auto phandle = RAIL_GetRxPacketInfo(gRail, RAIL_RX_PACKET_HANDLE_OLDEST_COMPLETE, &info);
    if(phandle == RAIL_RX_PACKET_HANDLE_INVALID) {
        Logger::Warning("failed to read packet!");
        return;
    }

    RAIL_GetRxPacketDetails(gRail, phandle, &details);

    if(kLogRx) {
        Logger::Notice("Rx(%u) rssi: %d", info.packetBytes, details.rssi);
    }

    // enqueue the packet (it will be copied)
    Packet::Handler::HandleRxPacket(info, details);
    gRxFrames++;

/*
    // generate acknowledgement
    static etl::array<uint8_t, 6> gAckBuffer;
    //etl::copy(, , gAckBuffer.begin());

    err = RAIL_WriteAutoAckFifo(gRail, gAckBuffer.data(), gAckBuffer.size());
    REQUIRE(err == RAIL_STATUS_NO_ERROR, "%s failed: %d", "RAIL_WriteAutoAckFifo");
*/

    // clean up
    RAIL_ReleaseRxPacket(gRail, phandle);
}

/**
 * @brief Begin transmission of the specified packet
 *
 * The packet's data is copied into the transmit FIFO, so the packet buffer may be released after
 * this call returns.
 *
 * @param packet Packet buffer to transmit
 *
 * @return 0 on success or a negative error code
 */
int Task::TxPacketImmediate(Packet::Handler::TxPacketBuffer *packet) {
    RAIL_Status_t err;

    taskENTER_CRITICAL();
    // write data into TX FIFO
    auto written = RAIL_WriteTxFifo(gRail, packet->data, packet->packetSize, true);
    if(written != packet->packetSize) {
        // TODO: log TX FIFO overflow
        gTxFifoDrops++;
        return -1;
    }

    // begin transmit
    if(kUseCca) {
        err = RAIL_StartCcaCsmaTx(gRail, gTxChannel, 0, &gCsmaConfig, nullptr);
    } else {
        err = RAIL_StartTx(gRail, gTxChannel, 0, nullptr);
    }
    if(err != RAIL_STATUS_NO_ERROR) {
        return -2;
    }

    // packet was queued for transmission :)
    gLastTx = packet;
    taskEXIT_CRITICAL();

    if(kLogTx) {
        Logger::Notice("start tx %p", packet);
    }

    return 0;
}

/**
 * @brief The last packet was successfully transmitted
 *
 * Release the packet buffer associated with the packet, and set up for transmitting the next
 * packet, if any.
 */
void Task::HandleTxComplete() {
    int err;

    // TODO: anything else?

    // discard the buffer
    Packet::Handler::DiscardTxPacket(gLastTx);
    gLastTx = nullptr;

    auto empty = Packet::Handler::GetTxEmptyFlag();

    // check if there's any other packets: if so, transmit one
    // XXX: is this potentially racy?
    if(!empty) {
        auto next = Packet::Handler::PopTxQueue();
        REQUIRE(next, "failed to pop tx packet (but queue isn't empty?)");

        err = TxPacketImmediate(next);
        REQUIRE(!err, "%s failed: %d", "TxPacketImmediate", err);
    }
}



/**
 * @brief Set the radio channel currently in use
 *
 * The effect of the change is immediate. Any pending data in the radio's receive and transmit
 * FIFOs will be lost, but packets that have already been downloaded into the packet handler are
 * unaffected. Similarly, any subsequently transmitted packets (such as ones still pending in
 * the packet handler's queues) will be sent on this new channel.
 *
 * @param newChannel New channel number to activate
 *
 * @return 0 on success
 */
int Task::SetChannel(const uint16_t newChannel) {
    RAIL_Status_t err;

    // validate channel number
    if(RAIL_IsValidChannel(gRail, newChannel) != RAIL_STATUS_NO_ERROR) {
        Logger::Warning("invalid channel %u", newChannel);
        return -2;
    }

    // reset FIFOs
    RAIL_ResetFifo(gRail, true, true);

    // start reception
    err = RAIL_StartRx(gRail, newChannel, nullptr);

    if(err != RAIL_STATUS_NO_ERROR) {
        Logger::Warning("%s failed: %d", "RAIL_StartRx", err);
        return -1;
    }

    gTxChannel = newChannel;
    return 0;
}

/**
 * @brief Read the currently active channel
 *
 * @return Current channel, or 0xFFFF if not configured/available
 */
uint16_t Task::GetChannel() {
    uint16_t temp;
    auto status = RAIL_GetChannel(gRail, &temp);
    if(status == RAIL_STATUS_NO_ERROR) {
        return temp;
    }

    return ~0;
}

/**
 * @brief Update the transmit power setting
 *
 * Change the power level used for transmission of all future packets. The actual transmit power
 * may be lower than what's requested.
 *
 * @param newPower New transmit power, in units of ⅒th dBm
 *
 * @return 0 on success, or a negative error code
 */
int Task::SetTxPower(const int16_t newPower) {
    auto raw = RAIL_ConvertDbmToRaw(gRail, RAIL_TX_POWER_MODE_SUBGIG, newPower);
    auto status = RAIL_SetTxPower(gRail, raw);
    if(status != RAIL_STATUS_NO_ERROR) {
        return -1;
    }
    return 0;
}

/**
 * @brief Get current transmit power setting
 *
 * Read out the current power amplifier level.
 *
 * @return Transmit power level, in units of ⅒th dBm
 */
int16_t Task::GetTxPower() {
    auto level = RAIL_GetTxPower(gRail);
    return RAIL_ConvertRawToDbm(gRail, RAIL_TX_POWER_MODE_SUBGIG, level);
}

/**
 * @brief Check if the radio is active
 *
 * The radio is considered active if it's tuned to a channel, and either in transmit or receive
 * mode.
 */
bool Task::IsActive() {
    auto state = RAIL_GetRadioState(gRail);
    return (state & RAIL_RF_STATE_RX) | (state & RAIL_RF_STATE_TX);
}



/**
 * @brief RAIL event thunk
 *
 * Invoked by RAIL whenever an event takes place; determine what happened and forward the event to
 * the radio task.
 *
 * @remark This may be invoked from an interrupt context, so ISR-safe FreeRTOS functions must be
 *         used to notify the caller.
 */
extern "C" void sl_rail_util_on_event(RAIL_Handle_t handle, RAIL_Events_t events) {
    BaseType_t woken{pdFALSE};

#if 0
    Logger::Notice("RAIL(%p) event: %016llx", handle, (uint64_t) events);
#endif

    // packet received
    if(events & RAIL_EVENT_RX_PACKET_RECEIVED) {
        // keep packet in FIFO until event is processed
        RAIL_HoldRxPacket(handle);
        xTaskNotifyIndexedFromISR(Task::gTask, Task::kNotificationIndex,
                Task::NotifyBits::PacketReceived, eSetBits, &woken);
    }
    // packet transmitted
    if(events & RAIL_EVENT_TX_PACKET_SENT) {
        Task::gTxFrames++;
        xTaskNotifyIndexedFromISR(Task::gTask, Task::kNotificationIndex,
                Task::NotifyBits::PacketTransmitted, eSetBits, &woken);
    }
    // packet failed to transmit (channel busy)
    if(events & RAIL_EVENT_TX_CHANNEL_BUSY) {
        Task::gTxCcaFails++;
        xTaskNotifyIndexedFromISR(Task::gTask, Task::kNotificationIndex,
                Task::NotifyBits::TxChannelBusy, eSetBits, &woken);
    }
    // RX frame error: CRC, block decode, and illegal frame length
    if(events & RAIL_EVENT_RX_FRAME_ERROR) {
        Task::gRxFrameErrors++;
    }
    // RX FIFO overflow: flush the RX FIFO
    if(events & RAIL_EVENT_RX_FIFO_OVERFLOW) {
        Task::gRxFifoOverflows++;
        RAIL_ResetFifo(handle, false, true);
    }

    // perform a pended context switch if needed
    portYIELD_FROM_ISR(woken);
}
