#include "em_device.h"
#include "sl_rail_util_dma.h"
#include "pa_conversions_efr32.h"
#include "sl_rail_util_rf_path.h"
#include "sl_rail_util_rssi.h"
#include "sl_rail_util_init.h"

#include <etl/array.h>

#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "Task.h"
#include "Init.h"

using namespace Radio;

/**
 * @brief Initialize the radio hardware and software.
 *
 * First, perform initialization of the RAIL stack, then spin up the processing tasks which may
 * perform further setup.
 */
void Radio::Init() {
    // configure interrupts
    constexpr static const etl::array<IRQn, 11> kRadioIrqs{{
        FRC_PRI_IRQn, FRC_IRQn, MODEM_IRQn, RAC_SEQ_IRQn, RAC_RSM_IRQn, BUFC_IRQn,
            AGC_IRQn, PROTIMER_IRQn, SYNTH_IRQn, RFECA0_IRQn, RFECA1_IRQn
    }};
    for(const auto irq : kRadioIrqs) {
        NVIC_SetPriority(irq, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1);
    }

    // set up the RAIL plugins and the library itself
    sl_rail_util_dma_init();
    sl_rail_util_pa_init();
    sl_rail_util_rf_path_init();
    sl_rail_util_rssi_init();

    sl_rail_util_init();
}

/**
 * @brief "RAIL initialization complete" callback
 *
 * Invoked by RAIL once its internal initialization completes. We'll use this to set up the radio
 * background work task.
 */
extern "C" void sl_rail_util_on_rf_ready(RAIL_Handle_t handle) {
    Task::Init(handle);
}

/**
 * @brief Handle a RAIL assertion
 */
extern "C" void sl_rail_util_on_assert_failed(RAIL_Handle_t handle, RAIL_AssertErrorCodes_t error) {
    Logger::Panic("RAIL(%p) assert: %08x", handle, error);
}
