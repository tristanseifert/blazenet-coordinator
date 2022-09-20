#include "Start.h"
#include "Rtos.h"
#include "Log/Logger.h"

#include "em_device.h"

using namespace Rtos;

/**
 * @brief Start the RTOS scheduler
 */
[[noreturn]] void Rtos::StartScheduler() {
    // configure NVIC
    NVIC_SetPriorityGrouping(0);

    // start FreeRTOS scheduler
    Logger::Debug("Starting scheduler");
    vTaskStartScheduler();

    // XXX: should never get here
    asm volatile("bkpt 0xf3" ::: "memory");
    while(1) {}
}
