#include "Drivers/sl_spidrv_instances.h"
#include "gecko-config/pin_config.h"
#include "sl_spidrv_eusart_host_config.h"

#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "Commands.h"
#include "IrqManager.h"
#include "Task.h"

using namespace HostIf;

TaskHandle_t Task::gTask;

bool Task::gCommandBufferValid{false};
CommandHeader Task::gCommandBuffer;
size_t Task::gPayloadBytesReceived{0};
etl::array<uint8_t, Task::kMaxPayloadSize> Task::gPayloadBuffer;

bool Task::gErrorFlag{false};

/**
 * @brief Initialize the host interface task
 */
void Task::Init() {
    static StaticTask_t gStorage;
    static StackType_t gStack[kStackSize];

    gTask = xTaskCreateStatic([](auto param) {
        Task::Main();
    }, kName.data(), kStackSize, nullptr, kPriority, gStack, &gStorage);
    REQUIRE(!!gTask, "failed to initialize %s", "host i/f task");
}

/**
 * @brief Task main loop
 */
void Task::Main() {
    BaseType_t ok;

    // perform deferred setup
    Logger::Trace("%s: init", "hostif");

    ReadCommand();

    // wait for event
    while(true) {
        uint32_t note;

        ok = xTaskNotifyWaitIndexed(kNotificationIndex, 0, TaskNotifyBits::All, &note,
                portMAX_DELAY);
        REQUIRE(ok == pdTRUE, "%s failed: %d", "xTaskNotifyWaitIndexed", ok);

        // received a command
        if(note & TaskNotifyBits::CmdReceiveComplete) {
            // handle command if valid
            if(gCommandBufferValid) {
                ProcessCommand();
            }
            // if not valid, simply receive another command
            else {
                Logger::Warning("Cmd not valid!");
                ReadCommand();
            }
        }
        // finished receiving command payload
        if(note & TaskNotifyBits::PayloadReceiveComplete) {
            // bail if the payload reading stage had a failure
            if(!gPayloadBytesReceived) {
                Logger::Warning("failed to read payload bytes");
                break;
            }

            // process the command with payload and set up to receive next one
            DispatchCommand(gCommandBuffer.command & ~0x80,
                    {gPayloadBuffer.data(), gPayloadBytesReceived});
            ReadCommand();
        }
        // finished transmitting command response; receive next command
        if(note & TaskNotifyBits::ResponseTransmitComplete) {
            ReadCommand();
        }
    }
}

/**
 * @brief Handle a received command
 *
 * If the command has no payload, it's dispatched immediately. Otherwise, we'll wait to receive
 * payload data from the host, or transmit the command's payload to it.
 *
 * @note Requires gCommandBufferValid == true
 */
void Task::ProcessCommand() {
    const auto cmd = gCommandBuffer.command & ~0x80;

    // process payload-ful command
    if(gCommandBuffer.payloadLength) {
        const bool isRead = (gCommandBuffer.command & 0x80);

        // host is expecting to read payload
        if(isRead) {
            // dispatch command, with an expectation of response
            DispatchCommandWithResponse(cmd, gCommandBuffer.payloadLength);
        }
        // host is writing payload
        else {
            ReadPayload(gCommandBuffer.payloadLength);
            gPayloadBytesReceived = 0;
        }
    }
    // otherwise, dispatch it immediately and set up to receive the next command
    else {
        DispatchCommand(cmd, {});
        ReadCommand();
    }
}

/**
 * @brief Dispatch a command, with optional payload
 *
 * Execute the comand specified, which may have an associated payload data part (if provided by
 * the host) as well.
 *
 * @param cmd Command to execute
 * @param payload Buffer holding the payload (if any)
 */
void Task::DispatchCommand(const uint8_t cmd, etl::span<uint8_t> payload) {
    int err;

    // ensure command is valid
    if(cmd > static_cast<uint8_t>(CommandId::NumCommands)) {
        Logger::Warning("Invalid cmd %02x", cmd);
        return;
    }

    // invoke handler
    const auto &handler = gHandlers[cmd];

    if(!TestFlags(handler.flags & HandlerFlags::SupportsWrite)) {
        Logger::Warning("Cmd %02x doesn't support %s", cmd, "write");
        return;
    }

    err = handler.write(cmd, payload);
    if(err) {
        Logger::Warning("Cmd %02x failed: %d", cmd, "write", err);
        gErrorFlag = true;
        IrqManager::Assert(Interrupt::CommandError);
    }
}

/**
 * @brief Execute a command, with a response part
 *
 * Execute the command specified and output the given number of bytes to the host.
 *
 * @param cmd Command to execute
 * @param numResponseBytes Number of bytes requested to read by host
 */
void Task::DispatchCommandWithResponse(const uint8_t cmd, const size_t numResponseBytes) {
    Ecode_t err;
    int ret;

    // ensure command is valid
    if(cmd > static_cast<uint8_t>(CommandId::NumCommands)) {
        Logger::Warning("Invalid cmd %02x", cmd);
        return;
    }

    // invoke handler
    const auto &handler = gHandlers[cmd];

    if(!TestFlags(handler.flags & HandlerFlags::SupportsRead)) {
        Logger::Warning("Cmd %02x doesn't support %s", cmd, "read");
        return;
    }

    ret = handler.read(cmd, numResponseBytes, gPayloadBuffer);
    if(ret < 0) {
        Logger::Warning("Cmd %02x failed: %d", cmd, "read", ret);
        gErrorFlag = true;
        IrqManager::Assert(Interrupt::CommandError);
        return;
    }
    REQUIRE(ret < gPayloadBuffer.size(), "invalid reply length: %d", ret);

    // send response
    err = SPIDRV_STransmit(sl_spidrv_eusart_host_handle, gPayloadBuffer.data(), ret,
            [](auto handle, auto status, auto numSent) {
        // notify task
        BaseType_t woken{pdFALSE};
        xTaskNotifyIndexedFromISR(gTask, kNotificationIndex,
                TaskNotifyBits::ResponseTransmitComplete, eSetBits, &woken);
        portYIELD_FROM_ISR(woken);
    }, 0);
    REQUIRE(err == ECODE_EMDRV_SPIDRV_OK, "%s failed: %d", "SPIDRV_STransmit %s", err, "response");
}



/**
 * @brief Set up a command data read
 *
 * This reads a two byte command structure from the SPI slave interface.
 */
void Task::ReadCommand() {
    Ecode_t err;

    // read the command header
    err = SPIDRV_SReceive(sl_spidrv_eusart_host_handle, &gCommandBuffer,
            sizeof(gCommandBuffer), [](auto handle, auto status, auto numReceived) {
        // check for success
        gCommandBufferValid = (status == ECODE_EMDRV_SPIDRV_OK) &&
            (numReceived == sizeof(gCommandBuffer));

        // notify the task the command was received
        BaseType_t woken{pdFALSE};
        xTaskNotifyIndexedFromISR(gTask, kNotificationIndex, TaskNotifyBits::CmdReceiveComplete,
                eSetBits, &woken);
        portYIELD_FROM_ISR(woken);
    }, 0);
    REQUIRE(err == ECODE_EMDRV_SPIDRV_OK, "%s failed: %d", "SPIDRV_SReceive %s", err, "header");

    // clear state
    gCommandBufferValid = false;
}

/**
 * @brief Set up a command payload read
 *
 * Read from the SPI the number of bytes specified in the second byte of the command structure.
 *
 * @param numBytes Number of payload bytes to read
 */
void Task::ReadPayload(const size_t numBytes) {
    Ecode_t err;

    // read the payload data
    err = SPIDRV_SReceive(sl_spidrv_eusart_host_handle, gPayloadBuffer.data(),
            etl::min(numBytes, gPayloadBuffer.size()),
            [](auto handle, auto status, auto numReceived) {
        if(status == ECODE_EMDRV_SPIDRV_OK) {
            gPayloadBytesReceived = numReceived;
        } else {
            gPayloadBytesReceived = 0;
        }

        // notify the task the data was received
        BaseType_t woken{pdFALSE};
        xTaskNotifyIndexedFromISR(gTask, kNotificationIndex,
                TaskNotifyBits::PayloadReceiveComplete, eSetBits, &woken);
        portYIELD_FROM_ISR(woken);
    }, 0);
    REQUIRE(err == ECODE_EMDRV_SPIDRV_OK, "%s failed: %d", "SPIDRV_SReceive %s", err,
            "payload");
}
