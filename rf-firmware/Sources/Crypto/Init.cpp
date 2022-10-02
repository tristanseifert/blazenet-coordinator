#include <em_cmu.h>
#include <sl_se_manager.h>

#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "Init.h"

using namespace Crypto;

/**
 * @brief Initialize the crypto subsystem
 *
 * This will set up the security engine on the chip, which is used to perform hardware accelerated
 * crypto operations.
 */
void Crypto::Init() {
    Logger::Notice("%s: init", "crypto");

    // enable clocks
    CMU_ClockEnable(cmuClock_SEMAILBOX, true);

    // set up security engine manager
    auto ok = sl_se_init();
    REQUIRE(ok == SL_STATUS_OK, "%s failed: %d", "sl_se_init", ok);
}
