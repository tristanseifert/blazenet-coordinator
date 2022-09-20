/**
 * @file
 *
 * @brief Application entry point
 */

#include "BuildInfo.h"
#include "Hw/Clocks.h"
#include "Hw/Indicators.h"
#include "Log/Logger.h"
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
}

/**
 * @brief Initialize hardware
 *
 * Set up high level peripherals and external hardware.
 */
static void HwInit() {
    Hw::Indicators::Init();
}

/**
 * @brief Firmware main routine
 *
 * Invoked by startup code after the C runtime is set up.
 */
extern "C" int main(int argc, void **argv) {
    EarlyInit();
    Logger::Notice("blazenet-rf firmware (%s-%s/%s) built on %s", gBuildInfo.gitBranch,
            gBuildInfo.gitHash, gBuildInfo.buildType, gBuildInfo.buildDate);

    HwInit();

    // create test thymer
    static TimerHandle_t fuckHandle;
    static StaticTimer_t fuckStorage;

    fuckHandle = xTimerCreateStatic("cum", pdMS_TO_TICKS(200), pdTRUE, nullptr,
            [](auto timerId) {
        static unsigned int cum{0};
/*
        if(cum & (1 << 0)) {
            GPIO_PinOutSet(LED_nRX_PORT, LED_nRX_PIN);
        } else {
            GPIO_PinOutClear(LED_nRX_PORT, LED_nRX_PIN);
        }

        if(cum & (1 << 1)) {
            GPIO_PinOutSet(LED_nTX_PORT, LED_nTX_PIN);
        } else {
            GPIO_PinOutClear(LED_nTX_PORT, LED_nTX_PIN);
        }

        if(cum & (1 << 2)) {
            GPIO_PinOutSet(LED_nATTN_PORT, LED_nATTN_PIN);
        } else {
            GPIO_PinOutClear(LED_nATTN_PORT, LED_nATTN_PIN);
        }

        cum++;

        // delay loop
        volatile unsigned int fuck{1420690};
        while(fuck--) {}
    }
    // start scheduler
    Rtos::StartScheduler();
}
