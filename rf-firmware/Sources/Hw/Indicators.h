#ifndef HW_INDICATORS_H
#define HW_INDICATORS_H

#include <stddef.h>
#include <stdint.h>

#include "Rtos/Rtos.h"

#include <etl/array.h>
#include <etl/span.h>
#include <etl/utility.h>

namespace Hw {
/**
 * @brief User indicator handler
 *
 * Controls the three radio related indicators on the front panel: RF attention, RF RX, and RF TX.
 * The attention indicator can be set to be off, solid on, or a blink pattern; while the RX and
 * TX indicators are pulse stretched.
 *
 * Under the hood, all three of these indicators are implemented with simple animation scripts,
 * which are run through until their end.
 */
class Indicators {
    private:
        /**
         * @brief Indicator state
         */
        struct State {
            /// Is this channel idle?
            uint32_t isIdle                     :1{1};
            /// Is the "next update" timestamp valid?
            uint32_t wantsTimerUpdate           :1{0};
            /// Current output state
            uint32_t isOn                       :1{0};

            /// Offset into the animation script (bytes)
            uint32_t animOffset                 :8{0};
            /// Pointer to animation script currently being used
            etl::span<const uint8_t> currentAnim;

            /// Tick timestamp at which this channel needs to be updated
            TickType_t nextUpdate;
        };

        /**
         * @brief Animation script commands
         *
         * Animation scripts consist of a sequence of commands, which are identified by a single
         * byte value as defined in the below enum. (Some commands may have one or more parameter
         * bytes following.)
         */
        enum AnimCommand: uint8_t {
            /// Skip the instruction
            NoOp                                = 0x00,

            /// Turn on the indicator
            TurnOn                              = 0x10,
            /// Turn off the indicator
            TurnOff                             = 0x11,

            /// Delay for X ticks (where X is the next byte)
            Delay                               = 0x20,

            /// Repeat the script from the beginning
            Restart                             = 0xFE,
            /// Indicates the end of the animation script
            End                                 = 0xFF,
        };

        /**
         * @brief Indicator names
         */
        enum class Indicator {
            Attention                           = 0,
            Tx                                  = 1,
            Rx                                  = 2,
        };

    private:
        /// A pair of (GPIO port, GPIO pin)
        using PinPair = etl::pair<uint32_t, uint8_t>;

    public:
        /// Total number of indicators (fixed)
        constexpr static const size_t kNumIndicators{3};

        static void Init();

        /**
         * @brief Pulse the transmit indicator
         */
        static inline void PulseTx() {
            SetChannelScript(Indicator::Tx, gAnimShortBlink);
        }

        /**
         * @brief Pulse the receive indicator
         */
        static inline void PulseRx() {
            SetChannelScript(Indicator::Rx, gAnimShortBlink);
        }

        /**
         * @brief Blink the attention indicator (slow)
         */
        static inline void BlinkAttentionSlow() {
            SetChannelScript(Indicator::Attention, gAnimAttentionBlinkSlow, true);
        }

    private:
        static void TimerFired();
        static void UpdateTimerPeriod();

        static void UpdateChannel(State &, const PinPair &, TickType_t &);
        static void SetChannelScript(const Indicator, etl::span<const uint8_t>,
                const bool = false);

        /**
         * @brief Set the state of an indicator
         */
        static inline void SetState(const PinPair &pin, const bool on) {
            if(on) {
                GPIO_PinOutClear(pin.first, pin.second);
            } else {
                GPIO_PinOutSet(pin.first, pin.second);
            }
        }

    private:
        /**
         * @brief Animation timer
         *
         * This timer is multiplexed between all three indicators to update their state.
         */
        static TimerHandle_t gTimer;

        /**
         * @brief State lock
         *
         * Used for mutual exclusion to ensure only one task updates the indicator state at a
         * time.
         */
        static SemaphoreHandle_t gLock;

        /**
         * @brief Indicator states
         *
         * Holds the current animation state of each of the indicators. They are ordered from
         * left to right: that is, index 0 is the attention signal, followed by the TX and RX
         * indicators' state.
         */
        static etl::array<State, kNumIndicators> gState;

        /**
         * @brief Mapping of indicator index to GPIO pins
         */
        static const etl::array<const PinPair, kNumIndicators> gIndicatorPins;

        /// Secrete logs when the timer period is updated
        constexpr static const bool kLogTimerUpdates{false};
        /// Secrete logs when channels are processed
        constexpr static const bool kLogChannelUpdates{false};

        /// Blink an indicator briefly
        constexpr static const etl::array<const uint8_t, 7> gAnimShortBlink{{
            AnimCommand::TurnOn,
            AnimCommand::Delay, pdMS_TO_TICKS(30),
            AnimCommand::TurnOff,
            AnimCommand::Delay, pdMS_TO_TICKS(30),
            AnimCommand::End,
        }};

        /// Repeatedly blink the indicator, with a 1Hz repetition rate
        constexpr static const etl::array<const uint8_t, 7> gAnimAttentionBlinkSlow{{
            AnimCommand::TurnOn,
            AnimCommand::Delay, pdMS_TO_TICKS(1000),
            AnimCommand::TurnOff,
            AnimCommand::Delay, pdMS_TO_TICKS(1000),
            AnimCommand::Restart,
        }};
};
};

#endif
