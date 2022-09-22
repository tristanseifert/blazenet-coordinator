#include <rail.h>

#include "Hw/Indicators.h"
#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "Task.h"

using namespace Radio;

TaskHandle_t Task::gTask;
RAIL_Handle_t Task::gRail;

size_t Task::gRxFifoOverflows{0};
size_t Task::gRxFrameErrors{0};
size_t Task::gRxFrames{0};

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
    RAIL_Status_t err;

    // perform deferred setup
    Logger::Trace("%s: init", "Radio");

    RAIL_ResetFifo(gRail, true, true);

    // XXX: begin reception
    err = RAIL_StartRx(gRail, 6, nullptr);
    REQUIRE(!err, "%s failed: %d", "RAIL_StartRx", err);

    // wait for event
    while(true) {
        uint32_t note;

        ok = xTaskNotifyWaitIndexed(kNotificationIndex, 0, NotifyBits::All, &note,
                portMAX_DELAY);
        REQUIRE(ok == pdTRUE, "%s failed: %d", "xTaskNotifyWaitIndexed", ok);

        // invoke the appropriate handlers
        if(note & NotifyBits::PacketReceived) {
            Hw::Indicators::PulseRx();

            // TODO: loop for all complete packets in FIFO
            ReadPacket();
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
    Logger::Notice("Rx(%u) rssi: %d", info.packetBytes, details.rssi);

    // copy packet payload and submit to packet handler
    // TODO: do this
    gRxFrames++;

    // clean up
    RAIL_ReleaseRxPacket(gRail, phandle);
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
