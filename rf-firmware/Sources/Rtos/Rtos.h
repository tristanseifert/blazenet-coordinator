/**
 * @file
 *
 * @brief RTOS helpers
 *
 * Various definitions (and umbrella include) for working with FreeRTOS
 */
#ifndef RTOS_RTOS_H
#define RTOS_RTOS_H

// pull in all the FreeRTOS stuff
#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>

#include <cmsis_os2.h>

namespace Rtos {
/**
 * @brief Firmware-specific priority level assignments
 *
 * Each entry in this enum defines the priority value for a particular "class" of tasks. This
 * ensures that important processing cannot get starved out by less important stuff.
 */
enum TaskPriority: UBaseType_t {
    /**
     * @brief Deferred interrupt calls
     */
    Dpc                                 = osPriorityISR,
    /**
     * @brief Driver work loops
     */
    Driver                              = osPriorityRealtime4,
    /**
     * @brief Supervisory tasks
     *
     * Any class of task responsible for making sure we don't self destruct: watchdog checkins,
     * thermal management, etc.
     */
    Supervisory                         = osPriorityRealtime,
    /**
     * @brief High priority app
     *
     * Application tasks that have a relatively higher priority, such as control loops.
     */
    AppHigh                             = osPriorityHigh,
    /**
     * @brief Middleware
     *
     * This includes stuff such as high-level protocol drivers (over the message passing interface)
     * and timers.
     */
    Middleware                          = osPriorityNormal,
    /**
     * @brief Low priority app
     *
     * Low priority application tasks, such as user interface or periodic recalibration.
     */
    AppLow                              = osPriorityBelowNormal,
    /**
     * @brief Idle
     *
     * Tasks that run when no other processing in the system is going on; useful for background
     * maintenance type tasks.
     */
    Background                          = osPriorityLow,
};
static_assert(TaskPriority::Background >= 0);
static_assert(TaskPriority::Background < configMAX_PRIORITIES);

/**
 * @brief Task notification indices
 *
 * System-wide reserved indices in the task notification array
 */
enum TaskNotifyIndex: size_t {
    /// reserved for FreeRTOS message buffer api
    Stream                              = 0,
    /**
     * @brief Notification bits reserved for driver and middleware use
     *
     * The assignment is as follows:
     * - Bit 0: confd service requests
     * - Bit 1: ResourceManager requests
     */
    DriverPrivate                       = 1,
    /// First task specific value
    TaskSpecific                        = 2,
};

/**
 * @brief Thread-local storage indices
 *
 * System-wide reserved indices for thread local storage.
 */
enum ThreadLocalIndex: size_t {
    /// Used by logging infrastructure
    TLSLogBuffer                        = 0,

    /// First task specific value
    TLSTaskSpecific                     = 1,
};
}

#endif
