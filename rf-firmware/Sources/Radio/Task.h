#ifndef RADIO_TASK_H
#define RADIO_TASK_H

#include <stddef.h>
#include <stdint.h>

#include "rail.h"

#include <etl/string_view.h>

#include "Rtos/Rtos.h"

namespace Radio {
/**
 * @brief Radio management task
 *
 * This is the FreeRTOS task that's responsible for handling events from the radio, dispatched to
 * us via RAIL's event notification mechanism. Additionally, this processes received packets, and
 * queues packets for transmission.
 */
class Task {
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
        enum TaskNotifyBits: uintptr_t {
            /// Bitwise OR of all supported task notification bits
            All                                 = (0xFFFFFFFFU),
        };

    public:
        static void Init(RAIL_Handle_t rail);

    private:
        static void Main();

    private:
        /// FreeRTOS Task handle
        static TaskHandle_t gTask;
        /// Radio interface handle (used for all later interfaces)
        static RAIL_Handle_t gRail;
};
}

#endif
