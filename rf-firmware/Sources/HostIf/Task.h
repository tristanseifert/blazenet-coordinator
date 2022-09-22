#ifndef HOSTIF_TASK_H
#define HOSTIF_TASK_H

#include <stddef.h>
#include <stdint.h>

#include <etl/array.h>
#include <etl/string_view.h>

#include "Rtos/Rtos.h"

namespace HostIf {
/**
 * @brief Host interface task
 *
 * Manages the SPI interface to the host, including message queuing and the register set available
 * to the host. It also controls the host interrupt line.
 */
class Task {
    private:
        /// Runtime priority level
        static const constexpr uint8_t kPriority{Rtos::TaskPriority::Middleware};
        /// Size of the task's stack, in words
        static const constexpr size_t kStackSize{420};
        /// Task name (for display purposes)
        static const constexpr etl::string_view kName{"HostIf"};
        /// Notification index
        static const constexpr size_t kNotificationIndex{Rtos::TaskNotifyIndex::TaskSpecific};

        /// Maximum payload size (bytes)
        static const constexpr size_t kMaxPayloadSize{256};

        /**
         * @brief Host command structure
         *
         * A small, packed structure received from the host. It indicates the command id and the
         * length of the payload.
         */
        struct Command {
            /// Command id
            uint8_t command;
            /// Number of payload bytes following the command
            uint8_t payloadLength;
        } __attribute__((packed));

    public:
        /**
         * @brief Task notification bit definitions
         *
         * Set from the background (such as an SPI transfer completion)
         */
        enum TaskNotifyBits: uintptr_t {
            /// Command reception complete
            CmdReceiveComplete                  = (1U << 0),
            /// Payload reception complete
            PayloadReceiveComplete              = (1U << 1),

            /// Bitwise OR of all supported task notification bits
            All                                 = (CmdReceiveComplete | PayloadReceiveComplete),
        };

    public:
        static void Init();

    private:
        static void Main();

        static void ReadCommand();
        static void ReadPayload(const size_t);

        /**
         * @brief Set the status of the host-facing IRQ line
         *
         * @param isAsserted Whether the interrupt is asserted (active low)
         */
        inline static void SetIrqStatus(const bool isAsserted) {
            if(isAsserted) {
                GPIO_PinOutClear(HOST_nIRQ_PORT, HOST_nIRQ_PIN);
            } else {
                GPIO_PinOutSet(HOST_nIRQ_PORT, HOST_nIRQ_PIN);
            }
        }

    private:
        /// FreeRTOS Task handle
        static TaskHandle_t gTask;

        /// Is the command buffer valid?
        static bool gCommandBufferValid;
        /// Command structure received from host
        static Command gCommandBuffer;
        /// Buffer for command payload
        static etl::array<uint8_t, kMaxPayloadSize> gPayloadBuffer;

};
}

#endif
