/**
 * @file
 *
 * @brief Application entry point
 */

#include "em_cmu.h"
#include "em_gpio.h"

#include "gecko-config/pin_config.h"

#include "BuildInfo.h"
#include "Log/Logger.h"
#include "Rtos/Rtos.h"
#include "Rtos/Start.h"

/**
 * @brief Firmware main routine
 *
 * Invoked by startup code after the C runtime is set up.
 */
extern "C" int main(int argc, void **argv) {
    // enable peripheral clocks
    CMU_ClockEnable(cmuClock_GPIO, true);

    // subsystem initialization
    Logger::Init();
    Logger::Notice("blazenet-rf firmware (%s-%s/%s) built on %s", gBuildInfo.gitBranch,
            gBuildInfo.gitHash, gBuildInfo.buildType, gBuildInfo.buildDate);

    // set up LEDs
    GPIO_PinModeSet(LED_nRX_PORT, LED_nRX_PIN, gpioModePushPull, true);
    GPIO_PinModeSet(LED_nTX_PORT, LED_nTX_PIN, gpioModePushPull, true);
    GPIO_PinModeSet(LED_nATTN_PORT, LED_nATTN_PIN, gpioModePushPull, false);


    unsigned int cum{0};
    while(1) {
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
