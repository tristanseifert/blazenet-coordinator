#ifndef RADIO_TASK_H
#define RADIO_TASK_H

#include <stddef.h>
#include <stdint.h>

#include "rail.h"

#include <etl/string_view.h>

#include "Rtos/Rtos.h"

extern "C" {
void sl_rail_util_on_event(RAIL_Handle_t, RAIL_Events_t);
};

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
        static const constexpr uint8_t kPriority{Rtos::TaskPriority::AppHigh};
        /// Size of the task's stack, in words
        static const constexpr size_t kStackSize{420};
        /// Task name (for display purposes)
        static const constexpr etl::string_view kName{"Radio"};
        /// Notification index
        static const constexpr size_t kNotificationIndex{Rtos::TaskNotifyIndex::TaskSpecific};

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

            /// Bitwise OR of all supported task notification bits
            All                                 = (PacketReceived),
        };

    public:
        static void Init(RAIL_Handle_t rail);

        static int SetChannel(const uint16_t newChannel);
        static uint16_t GetChannel();

        static bool IsActive();

    private:
        static void InitAutoAck();

        static void Main();

        static void ReadPacket();

    private:
        /// FreeRTOS Task handle
        static TaskHandle_t gTask;
        /// Radio interface handle (used for all later interfaces)
        static RAIL_Handle_t gRail;

        /// Performance counter to track the number of receive FIFO overflows
        static size_t gRxFifoOverflows;
        /// Number of frames received with errors (invalid CRC, etc.)
        static size_t gRxFrameErrors;
        /// Number of good frames received
        static size_t gRxFrames;
};
}

#endif
