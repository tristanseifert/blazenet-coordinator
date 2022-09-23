/**
 * @file
 *
 * @brief Application entry point
 */
#include "gpiointerrupt.h"
#include "Drivers/sl_spidrv_instances.h"
#include "Drivers/sl_uartdrv_instances.h"

#include "BuildInfo.h"
#include "Fs/Init.h"
#include "HostIf/Init.h"
#include "Hw/Clocks.h"
#include "Hw/Identity.h"
#include "Hw/Indicators.h"
#include "Log/Logger.h"
#include "Packet/Handler.h"
#include "Radio/Init.h"
#include "Rtos/Rtos.h"
#include "Rtos/Start.h"

#include <timers.h>

/**
 * @brief Perform early initialization
 *
 * This sets up basic low level system peripherals and subsystems in the firmware.
 */
static void EarlyInit() {
    // configure system clocks
    Hw::Clocks::Init();

    // set up logging
    Logger::Init();

    // read out system identity information
    Hw::Identity::Init();
}

/**
 * @brief Initialize hardware and drivers
 *
 * Set up high level peripherals and external hardware.
 */
static void HwInit() {
    Hw::Indicators::Init();

    GPIOINT_Init();

    sl_spidrv_init_instances();
    sl_uartdrv_init_instances();
}

/**
 * @brief Initialize firmware components
 *
 * This will set up the high level firmware components, including the radio stack and host
 * communication interfaces. The external flash filesystem is probed as well.
 */
static void SwInit() {
    Logger::Notice("blazenet-rf firmware (%s-%s/%s) built on %s", gBuildInfo.gitBranch,
            gBuildInfo.gitHash, gBuildInfo.buildType, gBuildInfo.buildDate);

    Fs::Init();

    // radio hardware/RAIL stack, and packet handler
    Packet::Handler::Init();
    Radio::Init();

    // host interface
    HostIf::Init();
}



/**
 * @brief Firmware main routine
 *
 * Invoked by startup code after the C runtime is set up.
 */
extern "C" int main(int argc, void **argv) {
    EarlyInit();
    HwInit();
    SwInit();


    // start scheduler
    Rtos::StartScheduler();
}
