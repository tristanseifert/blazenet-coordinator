#include "rail.h"

#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "Task.h"

using namespace Radio;

TaskHandle_t Task::gTask;
RAIL_Handle_t Task::gRail;

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
 * @brief Task main loop
 */
void Task::Main() {
    BaseType_t ok;

    // perform deferred setup
    Logger::Trace("%s: init", "Radio");

    // wait for event
    while(true) {
        uint32_t note;

        ok = xTaskNotifyWaitIndexed(kNotificationIndex, 0, TaskNotifyBits::All, &note,
                portMAX_DELAY);
        REQUIRE(ok == pdTRUE, "%s failed: %d", "xTaskNotifyWaitIndexed", ok);

        // TODO: implement
        Logger::Notice("Radio: %08x", note);
    }
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
    Logger::Notice("RAIL(%p) event: %016llx", handle, (uint64_t) events);
}
