#include "BlazeNet/Beacon.h"
#include "Hw/Indicators.h"
#include "Log/Logger.h"
#include "Rtos/Rtos.h"

#include "Watchdog.h"

using namespace HostIf;

TimerHandle_t Watchdog::gTimerHandle;

bool Watchdog::gCommsLostFlag{true};
size_t Watchdog::gCheckinsMissed{0};

/**
 * @brief Initialize the host communications watchdog
 */
void Watchdog::Init() {
    static StaticTimer_t gTimerStorage;

    gTimerHandle = xTimerCreateStatic("hostif comms wdog", pdMS_TO_TICKS(kWdogInterval), pdTRUE,
            nullptr, [](auto) {
        if(++gCheckinsMissed > kWdogThreshold && !gCommsLostFlag) {
            Logger::Warning("host comms lost!");
            HandleCommsLost();
        }
    }, &gTimerStorage);
    xTimerStart(gTimerHandle, 0);

    // XXX: initial failure state
    HandleCommsLost();
}

/**
 * @brief Handle loss of communications with the host
 *
 * Invoked in the context of the host interface watchdog timer.
 */
void Watchdog::HandleCommsLost() {
    gCommsLostFlag = true;
    Hw::Indicators::BlinkAttentionFast();

    // notify components
    BlazeNet::Beacon::CommsLost();
}

/**
 * @brief Handle communication being regained
 *
 * Invoked when the comms lost flag is set, but we just received a new command.
 */
void Watchdog::HandleCommsRegained() {
    gCommsLostFlag = false;
    Hw::Indicators::TurnOffAttention();

    BlazeNet::Beacon::CommsRegained();

    Logger::Notice("host comms regained");
}
