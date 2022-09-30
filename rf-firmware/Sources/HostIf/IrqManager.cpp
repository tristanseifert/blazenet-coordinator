#include <em_device.h>
#include <em_gpio.h>

#include "gecko-config/pin_config.h"

#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "IrqManager.h"

using namespace HostIf;

Interrupt IrqManager::gActive{Interrupt::None}, IrqManager::gMask{Interrupt::None},
          IrqManager::gMaskedActive{Interrupt::None};

bool IrqManager::gLostIrqRecovery{false};
uint16_t IrqManager::gTicksPending{0}, IrqManager::gPendingStage{0};

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

/**
 * @brief Initialize the IRQ manager
 *
 * Set up the external interrupt line
 */
void IrqManager::Init() {
    //GPIO_PinModeSet(HOST_nIRQ_PORT, HOST_nIRQ_PIN, gpioModeWiredAndPullUp, true);
    GPIO_PinModeSet(HOST_nIRQ_PORT, HOST_nIRQ_PIN, gpioModePushPull, true);
}

/**
 * @brief Update the state of the interrupt line
 *
 * Perform a logical AND between the active interrupts and the interrupt mask; if the result is
 * non-zero, assert the interrupt line.
 */
void IrqManager::Update() {
    taskENTER_CRITICAL();

    // perform update as normal
    auto result = gActive & gMask;
    const auto prev = gMaskedActive;
    const auto changed = (result != gMaskedActive);

    if(kToggleIrqLine) {
        // toggle IRQ line
        if(changed) {
            GPIO_PinOutToggle(HOST_nIRQ_PORT, HOST_nIRQ_PIN);
        }
    } else {
        // IRQ line is level active
        if(!gLostIrqRecovery) {
            SetIrqStatus(TestFlags(result));
        }
    }
    gMaskedActive = result;

    taskEXIT_CRITICAL();

    // optional logging later
    if(changed) {
        if(kLogChanges) {
            Logger::Notice("IRQ: %08x -> %08x", prev, result);
        }
    }
}

/**
 * @brief Tick callback
 *
 * This checks how long an interrupt has been pending for; if it's been more than a certain number
 * of ticks, we'll pulse the interrupt line. This makes up for a host losing interrupts due to
 * agressive filtering.
 */
void IrqManager::TickCallback() {
    if((TestFlags(gMaskedActive) && ++gTicksPending > kIrqThreshold) || gLostIrqRecovery) {
        taskENTER_CRITICAL();

        // stage 0: de-assert irq line
        if(gPendingStage == 0) {
            SetIrqStatus(false);

            gLostIrqRecovery = true;
            gPendingStage = 1;
        }
        else if(gPendingStage == 1 || gPendingStage == 2) {
            gPendingStage++;
        }
        // stage 1: re-assert irq line
        else if(gPendingStage == 3) {
            SetIrqStatus(true);
            gPendingStage = 3;
        }
        // stage 2: profit
        else if(gPendingStage == 4) {
            gLostIrqRecovery = false;
            gTicksPending = 0;
            gPendingStage = 0;
        }

        taskEXIT_CRITICAL();
    }
}
