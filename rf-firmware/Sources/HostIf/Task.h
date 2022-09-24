#ifndef HOSTIF_TASK_H
#define HOSTIF_TASK_H

#include <stddef.h>
#include <stdint.h>

#include <etl/array.h>
#include <etl/span.h>
#include <etl/string_view.h>

#include "bitflags.h"
#include "Rtos/Rtos.h"

namespace HostIf {
namespace Response {
struct GetStatus;
}
namespace Handlers {
struct GetStatus;
}

/**
 * @brief Flags for command handlers
 *
 * These flags are used in the command handler table to determine various capabilities of a
 * command handler.
 */
enum class HandlerFlags: uintptr_t {
    /// Does the handler support reads?
    SupportsRead                                = (1 << 0),
    /// Does the handler support writes?
    SupportsWrite                               = (1 << 1),
};
ENUM_FLAGS_EX(HandlerFlags, uintptr_t);

/**
 * @brief Host interface task
 *
 * Manages the SPI interface to the host, including message queuing and the register set available
 * to the host. It also controls the host interrupt line.
 */
class Task {
    friend struct Handlers::GetStatus;

    private:
        /// Runtime priority level
        static const constexpr uint8_t kPriority{Rtos::TaskPriority::AppHigh};
        /// Size of the task's stack, in words
        static const constexpr size_t kStackSize{420};
        /// Task name (for display purposes)
        static const constexpr etl::string_view kName{"HostIf"};
        /// Notification index
        static const constexpr size_t kNotificationIndex{Rtos::TaskNotifyIndex::TaskSpecific};

        /// Maximum payload size (bytes)
        static const constexpr size_t kMaxPayloadSize{256};
        /// Maximum supported command id (TODO: keep in sync with CommandId enum)
        static const constexpr size_t kMaxCommandId{0x09};

        /**
         * @brief Command handler
         *
         * Defines a handler for a host command. These commands are implemented by means of
         * callbacks executed by the worker task to produce or receive data. Callbacks must be as
         * short as possible to avoid occupying the handler task, which would prevent further
         * commands from being handled.
         */
        struct CommandHandler {
            /// Flags
            HandlerFlags flags;

            /**
             * @brief Read callback
             *
             * Invoked when the host requests this command and desires to read back data. The
             * handler shall fill the provided buffer.
             *
             * @return Number of bytes actually written to buffer, or a negative error code
             */
            int (*read)(const uint8_t, const size_t, etl::span<uint8_t>);

            /**
             * @brief Write callback
             *
             * Invoked when the host executes this command and provides a payload.
             *
             * @return 0 on success, or a negative error code
             */
            int (*write)(const uint8_t, etl::span<const uint8_t>);
        };

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
            /// Response transmission complete
            ResponseTransmitComplete            = (1U << 2),

            /// Bitwise OR of all supported task notification bits
            All                                 = (CmdReceiveComplete | PayloadReceiveComplete |
                    ResponseTransmitComplete),
        };

    public:
        static void Init();

    private:
        static void Main();
        static void ProcessCommand();
        static void DispatchCommand(const uint8_t, etl::span<uint8_t>);
        static void DispatchCommandWithResponse(const uint8_t, const size_t);

        static void ReadCommand();
        static void ReadPayload(const size_t);

    private:
        /// Global command handlers
        static const etl::array<const CommandHandler, kMaxCommandId> gHandlers;

        /// FreeRTOS Task handle
        static TaskHandle_t gTask;

        /// Is the command buffer valid?
        static bool gCommandBufferValid;
        /// Command structure received from host
        static struct CommandHeader gCommandBuffer;
        /// Number of payload bytes received
        static size_t gPayloadBytesReceived;
        /// Buffer for command payload (shared between rx and tx)
        static etl::array<uint8_t, kMaxPayloadSize> gPayloadBuffer;

        /// Error flag (set if the last command returned an error; cleared on status read)
        static bool gErrorFlag;
};
}

#endif
