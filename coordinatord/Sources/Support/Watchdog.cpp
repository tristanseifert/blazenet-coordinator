#include <cerrno>
#include <cstdint>
#include <system_error>

#include <plog/Log.h>

#include "Support/Watchdog.h"

using namespace Support;

bool Watchdog::gIsActive{false};
std::chrono::microseconds Watchdog::gInterval;

#ifdef CONF_WITH_SYSTEMD
/*
 * Linux-specific systemd watchdog implementation
 */
#include <systemd/sd-daemon.h>

/**
 * @brief Determine watchdog state
 *
 * Query systemd to discover the watchdog state for this process, and the interval at which it
 * needs to be notified.
 */
void Watchdog::Init() {
    uint64_t usec{0};
    int err;

    err = sd_watchdog_enabled(0, &usec);

    // watchdog is enabled
    if(err > 0) {
        gInterval = std::chrono::microseconds(usec);
        gIsActive = true;
    }
    // success, but watchdog not required
    else if(!err) {
        gIsActive = false;
    }
    // oopsie!
    else {
        throw std::system_error(errno, std::generic_category(), "sd_watchdog_enabled");
    }

    PLOG_DEBUG << "Watchdog is " << (gIsActive ? "enabled" : "disabled") << ", interval "
               << gInterval.count() << " µS";
}

/**
 * @brief Enable watchdog monitoring
 *
 * This notifies systemd that the service has fully started up, and is ready to accept requests,
 * and in turn should start being supervised.
 */
void Watchdog::Start() {
    PLOG_DEBUG << "sd_notify ready";
    sd_notify(0, "READY=1");
}

/**
 * @brief Disable watchdog monitoring
 *
 * This notifies systemd we're beginning shutdown.
 */
void Watchdog::Stop() {
    PLOG_DEBUG << "sd_notify stopping";
    sd_notify(0, "STOPPING=1");
}

/**
 * @brief Kick the watchdog
 */
void Watchdog::Kick() {
    if(gIsActive) {
        sd_notify(0, "WATCHDOG=1");
    }
}
#else
/*
 * Watchdog stubs
 */
#warning This platform does not implement watchdog support

void Watchdog::Init() {
    PLOG_WARNING << "Watchdog not supported";
}
void Watchdog::Start() {

}
void Watchdog::Stop() {

}
void Watchdog::Kick() {

}

#endif
