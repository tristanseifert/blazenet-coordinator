#include "Drivers/sl_spidrv_instances.h"
#include "gecko-config/pin_config.h"
#include "sl_spidrv_eusart_host_config.h"

#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "Task.h"

using namespace HostIf;

TaskHandle_t Task::gTask;

bool Task::gCommandBufferValid{false};
Task::Command Task::gCommandBuffer;
etl::array<uint8_t, Task::kMaxPayloadSize> Task::gPayloadBuffer;

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

        // handle the notification
        Logger::Notice("hostif: %08x", note);

        if(note & TaskNotifyBits::CmdReceiveComplete) {
            // handle command if valid
            if(gCommandBufferValid) {
                Logger::Notice("Cmd: %02x, %u bytes", gCommandBuffer.command,
                        gCommandBuffer.payloadLength);

                // TODO: read payload, if needed
                if(gCommandBuffer.payloadLength) {
                    ReadPayload(gCommandBuffer.payloadLength);
                } else {
                    // process the command

                    // receive next command
                    ReadCommand();
                }
            }
            // if not valid, simply receive another command
            else {
                ReadCommand();
            }
        }
    }
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
        Logger::Notice("SPI rx(%d): cmd %u bytes", status, numReceived);

        // check for success
        gCommandBufferValid = (status == ECODE_EMDRV_SPIDRV_OK) &&
            (numReceived == sizeof(gCommandBuffer));

        // notify the task the command was received
        BaseType_t woken{pdFALSE};
        xTaskNotifyIndexedFromISR(gTask, kNotificationIndex, TaskNotifyBits::CmdReceiveComplete,
                eSetBits, &woken);
        portYIELD_FROM_ISR(woken);
    }, 0);
    REQUIRE(err == ECODE_EMDRV_SPIDRV_OK, "%s failed: %d", "SPIDRV_SReceive", err);
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
        Logger::Notice("SPI rx(%d): payload %u bytes", status, numReceived);

        // notify the task the data was received
        BaseType_t woken{pdFALSE};
        xTaskNotifyIndexedFromISR(gTask, kNotificationIndex,
                TaskNotifyBits::PayloadReceiveComplete, eSetBits, &woken);
        portYIELD_FROM_ISR(woken);
    }, 0);
    REQUIRE(err == ECODE_EMDRV_SPIDRV_OK, "%s failed: %d", "SPIDRV_SReceive", err);
}
