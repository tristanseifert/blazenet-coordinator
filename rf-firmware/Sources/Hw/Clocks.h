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
            // enable peripheral blocks
            CMU_ClockEnable(cmuClock_GPIO, true);
        }
};
}

#endif
