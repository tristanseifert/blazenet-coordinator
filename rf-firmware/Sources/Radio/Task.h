#ifndef RADIO_TASK_H
#define RADIO_TASK_H

#include <stddef.h>
#include <stdint.h>

#include <rail.h>

#include <etl/string_view.h>

#include "Packet/Handler.h"
#include "Rtos/Rtos.h"

extern "C" {
void sl_rail_util_on_event(RAIL_Handle_t, RAIL_Events_t);
};

namespace HostIf::Response {
struct GetCounters;
}

namespace Radio {
/**
 * @brief Radio management task
 *
 * This is the FreeRTOS task that's responsible for handling events from the radio, dispatched to
 * us via RAIL's event notification mechanism. Additionally, this processes received packets, and
 * queues packets for transmission.
 */
class Task {
    friend void ::sl_rail_util_on_event(RAIL_Handle_t, RAIL_Events_t);

    private:
        /// Runtime priority level
        static const constexpr uint8_t kPriority{Rtos::TaskPriority::Middleware};
        /// Size of the task's stack, in words
        static const constexpr size_t kStackSize{420};
        /// Task name (for display purposes)
        static const constexpr etl::string_view kName{"Radio"};
        /// Notification index
        static const constexpr size_t kNotificationIndex{Rtos::TaskNotifyIndex::TaskSpecific};

        /// Should received packets be logged?
        static const constexpr bool kLogRx{false};
        /// Should transmit packets be logged?
        static const constexpr bool kLogTx{false};
        /// Should CSMA transmit failures be logged?
        static const constexpr bool kLogTxCsmaRetries{true};

        /**
         * @brief Enable clear channel assessment before transmit
         *
         * When set, CSMA is used to ensure the channel is clear before transmitting.
         */
        static const constexpr bool kUseCca{true};
        /// Maximum number of CSMA failures before packet is dropped
        static const constexpr size_t kMaxCsmaFails{5};

    private:
        /**
         * @brief Task notification bit definitions
         *
         * Whenever there's a radio-related ~ thing ~ to do, we'll send a notification to the
         * radio task. Many of these will come directly from RAIL.
         */
        enum NotifyBits: uintptr_t {
            /// A new packet has been received and is available
            PacketReceived                      = (1 << 0),
            /// The last packet was transmitted successfully
            PacketTransmitted                   = (1 << 1),
            /// Failed to transmit a packet, because the channel is busy (retry later)
            TxChannelBusy                       = (1 << 2),
            /// Radio must be calibrated as soon as possible
            CalibrationRequired                 = (1 << 3),

            /// Bitwise OR of all supported task notification bits
            All                                 = (PacketReceived | PacketTransmitted |
                    TxChannelBusy | CalibrationRequired),
        };

    public:
        static void Init(RAIL_Handle_t rail);

        static int SetChannel(const uint16_t newChannel);
        static uint16_t GetChannel();
        static int SetTxPower(const int16_t newPower);
        static int16_t GetTxPower();

        static bool IsActive();

        [[nodiscard]] static int TxPacketImmediate(Packet::Handler::TxPacketBuffer *packet);

        static void ReadCounters(HostIf::Response::GetCounters *packet);

    private:
        static void InitAutoAck();
        static void InitCalibration();

        static void Main();

        static void ReadPacket();
        static void HandleTxComplete();

    private:
        /// CSMA configuration
        static const RAIL_CsmaConfig_t gCsmaConfig;

        /// FreeRTOS Task handle
        static TaskHandle_t gTask;
        /// Radio interface handle (used for all later interfaces)
        static RAIL_Handle_t gRail;

        /// Radio calibration data
        static RAIL_CalValues_t gCalibrationData;
        /// Image rejection calibration value
        static uint32_t gCalibrationIr;

        /// Performance counter to track the number of receive FIFO overflows
        static size_t gRxFifoOverflows;
        /// Number of frames received with errors (invalid CRC, etc.)
        static size_t gRxFrameErrors;
        /// Number of good frames received
        static size_t gRxFrames;

        /// Number of packets that failed to be transmitted because TX FIFO is full
        static size_t gTxFifoDrops;
        /// Number of times the channel occupancy check prevented transmission
        static size_t gTxCcaFails;
        /// Number of packets transmitted successfully
        static size_t gTxFrames;

        /// Channel to transmit on
        static uint16_t gTxChannel;
        /// Last frame transmitted
        static Packet::Handler::TxPacketBuffer *gLastTx;
};
}

#endif
