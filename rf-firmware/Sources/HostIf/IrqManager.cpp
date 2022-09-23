#include <em_device.h>
#include <em_gpio.h>

#include "gecko-config/pin_config.h"

#include "Log/Logger.h"

#include "IrqManager.h"

using namespace HostIf;

Interrupt IrqManager::gActive{Interrupt::None}, IrqManager::gMask{Interrupt::None};

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
    auto result = gActive & gMask;
    SetIrqStatus(TestFlags(result));
}
