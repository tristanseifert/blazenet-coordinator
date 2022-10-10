#include "em_device.h"
#include "em_gpio.h"
#include "gecko-config/pin_config.h"

#include <etl/optional.h>

#include "Log/Logger.h"
#include "Rtos/Rtos.h"
#include "Indicators.h"

using namespace Hw;

TimerHandle_t Indicators::gTimer{nullptr};
SemaphoreHandle_t Indicators::gLock{nullptr};

etl::array<Indicators::State, Indicators::kNumIndicators> Indicators::gState;

const etl::array<const Indicators::PinPair, Indicators::kNumIndicators>
Indicators::gIndicatorPins{{
    {LED_nATTN_PORT, LED_nATTN_PIN},
    {LED_nTX_PORT, LED_nTX_PIN},
    {LED_nRX_PORT, LED_nRX_PIN},
}};


/**
 * @brief Initialize the indicator handler
 *
 * It initializes the timer for the receive and transmit indicators' pulse stretching and
 * configures the GPIOs.
 */
void Indicators::Init() {
    // set up LED GPIOs (they are all off)
    GPIO_PinModeSet(LED_nRX_PORT, LED_nRX_PIN, gpioModePushPull, true);
    GPIO_PinModeSet(LED_nTX_PORT, LED_nTX_PIN, gpioModePushPull, true);
    GPIO_PinModeSet(LED_nATTN_PORT, LED_nATTN_PIN, gpioModePushPull, true);

    // initialize the timer
    static StaticTimer_t gTimerStorage;

    gTimer = xTimerCreateStatic("Blinkenlights", 1, pdFALSE, nullptr,
            [](auto timerId) {
        Indicators::TimerFired();
    }, &gTimerStorage);
    REQUIRE(!!gTimer, "failed to initialize %s", "indicator timer");

    // initialize lock guarding our internal state
    static StaticSemaphore_t gLockStorage;

    gLock = xSemaphoreCreateMutexStatic(&gLockStorage);
    REQUIRE(!!gLock, "failed to initialize %s", "indicator lock");

    // set up the "self test" LED pattern (all indicators are on for 500ms)
    SetChannelScript(Indicator::Attention, gAnimLongBlink, true);
    SetChannelScript(Indicator::Rx, gAnimLongBlink, true);
    SetChannelScript(Indicator::Tx, gAnimLongBlink, true);
}

/**
 * @brief Process a timer event
 *
 * The timer fired, so update the animation state machine for all indicators that are due.
 */
void Indicators::TimerFired() {
    const auto now = xTaskGetTickCount();
    if(kLogChannelUpdates) {
        Logger::Notice("Timer fired: %u", now);
    }

    // grab the state lock
    xSemaphoreTake(gLock, portMAX_DELAY);

    // process each channel
    for(size_t i = 0; i < gState.size(); i++) {
        auto &channel = gState[i];

        // skip idle channels
        if(channel.isIdle || !channel.wantsTimerUpdate) {
            continue;
        }
        // skip if not yet the timstamp
        else if(channel.nextUpdate > now) {
            // TODO: handle wrap-around
            continue;
        }

        // handle the channel's updating
        if(kLogChannelUpdates) {
            Logger::Notice("* ch %u: %u", i, channel.nextUpdate);
        }

        TickType_t next;
        UpdateChannel(channel, gIndicatorPins[i], next);

        if(next) {
            channel.nextUpdate = now + next;
        }
    }

    // re-arm the timer for next time
    UpdateTimerPeriod();

    // releae the state lock
    xSemaphoreGive(gLock);
}

/**
 * @brief Update timer expiration
 *
 * Iterate through all indicators' state, and update the timer such that it will fire for the first
 * indicator that needs it.
 *
 * @note Requires the state lock is held when invoked.
 */
void Indicators::UpdateTimerPeriod() {
    etl::optional<TickType_t> nextUpdate;
    const auto now = xTaskGetTickCount();

    for(const auto &channel : gState) {
        if(channel.isIdle || !channel.wantsTimerUpdate) {
            continue;
        }

        const auto fromNow = channel.nextUpdate - now;

        if(kLogTimerUpdates) {
            Logger::Notice("Channel %p: %u %u %u", &channel, channel.nextUpdate, fromNow, now);
        }

        // no previous time, so use it
        if(!nextUpdate) {
            nextUpdate = fromNow;
        }
        // otherwise, check that it's the lowest time
        else {
            if(*nextUpdate > fromNow) {
                nextUpdate = fromNow;
            }
        }
    }

    if(nextUpdate) {
        // make sure period is _at least_ 1; otherwise shit breaks
        const auto actualPeriod = etl::max(*nextUpdate, 1U);
        if(kLogTimerUpdates) {
            Logger::Notice("> period %d", actualPeriod);
        }

        xTimerChangePeriod(gTimer, actualPeriod, 0);
    }
}



/**
 * @brief Update the state of an indicator
 *
 * @param state Indicator state
 * @param pin GPIO pins the indicator is connected to
 * @param outNextUpdate Variable to receive the number of ticks until the next update
 */
void Indicators::UpdateChannel(State &state, const PinPair &pin, TickType_t &outNextUpdate) {
    size_t off{state.animOffset};

    while(off < state.currentAnim.size()) {
        // read the command
        const auto cmd = state.currentAnim[off++];
        switch(cmd) {
            // turn on the indicator
            case AnimCommand::TurnOn:
                state.isOn = true;
                SetState(pin, true);
                break;
            // turn off the indicator
            case AnimCommand::TurnOff:
                state.isOn = false;
                SetState(pin, false);
                break;

            // delay
            case AnimCommand::Delay:
                outNextUpdate = state.currentAnim[off++];
                state.animOffset = off;
                state.wantsTimerUpdate = true;
                return;

            // Restart the script from the beginning
            case AnimCommand::Restart:
                state.animOffset = off = 0;
                break;
            // end of script
            case AnimCommand::End:
                goto scriptFinished;

            // no-op (ignored)
            case AnimCommand::NoOp:
                break;
            // unknown command
            default:
                Logger::Warning("unknown anim cmd $%02x (at %p+%d)", cmd, state.currentAnim.data(),
                        (off - 1));
                goto scriptFinished;
        }
    }

scriptFinished:;
    // if we drop down here, the animation script is over
    state.isIdle = true;
    state.wantsTimerUpdate = false;
    state.nextUpdate = 0;
}

/**
 * @brief Set a channel's state to the given animation script
 *
 * Update the state of an output channel such that it will begin processing the provided
 * animation script.
 *
 * @param which Indicator whose state to update
 * @param script Animation script to set
 * @param immediate When set, any existing animation is interrupted (otherwise the call is a no-op)
 */
void Indicators::SetChannelScript(const Indicator which, etl::span<const uint8_t> script,
        const bool immediate) {
    // get the lock on the state
    xSemaphoreTake(gLock, portMAX_DELAY);
    auto &state = gState[static_cast<size_t>(which)];

    // bail out if not immediate and channel is active
    if(!immediate && !state.isIdle) {
        xSemaphoreGive(gLock);
        return;
    }

    // update the channel state
    state.wantsTimerUpdate = false;
    state.isIdle = false;
    state.animOffset = 0;
    state.currentAnim = script;
    state.nextUpdate = 0;

    // run the animation script until the next wait
    TickType_t nextDelay{0};
    UpdateChannel(state, gIndicatorPins[static_cast<size_t>(which)], nextDelay);

    if(nextDelay) {
        // ignore wrap-around
        state.wantsTimerUpdate = true;
        state.nextUpdate = xTaskGetTickCount() + nextDelay;
    } else {
        state.wantsTimerUpdate = false;
    }

    // update the timer period, then drop the lock
    UpdateTimerPeriod();

    xSemaphoreGive(gLock);
}
