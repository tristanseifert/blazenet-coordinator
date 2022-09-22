#include "Drivers/sl_spidrv_instances.h"
#include "gecko-config/pin_config.h"
#include "sl_spidrv_eusart_host_config.h"

#include "Log/Logger.h"

#include "Task.h"
#include "Init.h"

using namespace HostIf;

/**
 * @brief Initialize the SPI host interface
 *
 * This will set up the hardware (GPIOs) and then starts the SPI processing task.
 */
void HostIf::Init() {
    // set up GPIOs (open drain, w/ pull-up)
    GPIO_PinModeSet(HOST_nIRQ_PORT, HOST_nIRQ_PIN, gpioModeWiredAndPullUp, true);

    // start the task
    Task::Init();
}
