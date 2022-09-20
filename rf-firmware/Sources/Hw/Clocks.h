#ifndef HW_CLOCKS_H
#define HW_CLOCKS_H

#include "em_cmu.h"

namespace Hw {
/**
 * @brief Clock management
 */
class Clocks {
    public:
        /**
         * @brief Enable all system clocks required for startup
         */
        static void Init() {
            // DMA engines
            CMU_ClockEnable(cmuClock_LDMA, true);
            CMU_ClockEnable(cmuClock_LDMAXBAR, true);

            // generic IO stuff
            CMU_ClockEnable(cmuClock_GPIO, true);

            // EUSART0: SPI NOR flash
            CMU_ClockEnable(cmuClock_EUSART0, true);
            // EUSART1: TTY to host
            CMU_ClockEnable(cmuClock_EUSART1, true);
            // EUSART2: SPI to host
            CMU_ClockEnable(cmuClock_EUSART2, true);
        }
};
}

#endif
