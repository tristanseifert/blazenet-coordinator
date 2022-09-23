#ifndef HOSTIF_WATCHDOG_H
#define HOSTIF_WATCHDOG_H

namespace HostIf {
/**
 * @brief Host communications watchdog
 *
 * This is a software watchdog timer that ensures that we periodically receive commands from the
 * host. If the timer expires, we can disable autonomous periodic background jobs and enter an
 * error state.
 */
class Watchdog {
    private:
        /// Watchdog interval (in ms)
        static const constexpr uintptr_t kWdogInterval{2500};
        /// Number of watchdog intervals without commands before alarming
        static const constexpr size_t kWdogThreshold{6};

    public:
        static void Init();

        /**
         * @brief Kick the watchdog timer to prevent it from expiring
         *
         * Notifies the watchdog that a command has been executed; if this is the first command
         * back again, it re-enables periodic stuff that was disabled.
         */
        static inline void Kick() {
            if(gCommsLostFlag) {
                HandleCommsRegained();
            }
            gCheckinsMissed = 0;
        }

    private:
        static void HandleCommsLost();
        static void HandleCommsRegained();

    private:
        /// Communication lost flag (set if time elapses and no packet from host)
        static bool gCommsLostFlag;
        /// Number of watchdog iterations without a received message
        static size_t gCheckinsMissed;
        /// Communications watchdog timer
        static TimerHandle_t gTimerHandle;
};
}

#endif
