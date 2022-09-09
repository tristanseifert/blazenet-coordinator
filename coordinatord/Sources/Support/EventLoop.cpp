#include <event2/event.h>
#include <plog/Log.h>

#include <atomic>
#include <cerrno>

#include "Support/EventLoop.h"
#include "Support/Watchdog.h"

using namespace Support;

// declared in main.cpp
extern std::atomic_bool gRun;

/**
 * @brief Current event loop
 *
 * This is a thread-local that is set to the `this` value when we start executing a run loop.
 */
thread_local std::weak_ptr<EventLoop> EventLoop::gCurrentEventLoop;



/**
 * @brief Initialize the event loop
 *
 * @param isMainLoop Whether this is the main loop (which handles watchdog events and signals)
 */
EventLoop::EventLoop(const bool isMainLoop) {
    // create the event base
    this->evbase = event_base_new();
    if(!this->evbase) {
        throw std::runtime_error("failed to allocate event_base");
    }

    // add default event sources
    if(isMainLoop) {
        this->initWatchdogEvent();
        this->initSignalEvents();
    }
}

/**
 * @brief Release event loop resources
 *
 * @remark The event loop should be stopped when destroying.
 */
EventLoop::~EventLoop() {
    // release events
    for(auto ev : this->signalEvents) {
        if(!ev) {
            continue;
        }
        event_free(ev);
    }

    if(this->watchdogEvent) {
        event_free(this->watchdogEvent);
    }

    event_base_free(this->evbase);
}

/**
 * @brief Get the current thread's event loop
 *
 * Return the event loop that most recently executed on this thread. If no event loop exists, it
 * will return `nullptr`.
 */
std::shared_ptr<EventLoop> EventLoop::Current() {
    return gCurrentEventLoop.lock();
}

/**
 * @brief Create watchdog event
 *
 * Create a timer event with half of the period of the watchdog timer. Every time the event fires,
 * it will kick the watchdog to ensure we don't get killed.
 */
void EventLoop::initWatchdogEvent() {
    // bail if watchdog is disabled
    if(!Watchdog::IsActive()) {
        PLOG_VERBOSE << "watchdog disabled, skipping event creation";
        return;
    }

    // get interval
    const auto usec = Watchdog::GetInterval().count() / 2;

    // create and add event
    this->watchdogEvent = event_new(this->evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        Watchdog::Kick();
    }, this);
    if(!this->watchdogEvent) {
        throw std::runtime_error("failed to allocate watchdog event");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(usec / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(usec % 1'000'000U),
    };

    evtimer_add(this->watchdogEvent, &tv);
}

/**
 * @brief Create termination signal events
 *
 * Create an event that watches for POSIX signals that indicate we should exit: specifically, this
 * is SIGINT, SIGTERM, and SIGHUP.
 */
void EventLoop::initSignalEvents() {
    size_t i{0};

    for(const auto signum : kEvents) {
        auto ev = evsignal_new(this->evbase, signum, [](auto fd, auto what, auto ctx) {
            reinterpret_cast<EventLoop *>(ctx)->handleTermination();
        }, this);
        if(!ev) {
            throw std::runtime_error("failed to allocate signal event");
        }

        event_add(ev, nullptr);
        this->signalEvents[i++] = ev;
    }
}

/**
 * @brief Run event loop
 *
 * Process events on the event loop.
 *
 * @remark This will sit here basically forever; kicking of the watchdog is implemented by means
 *         of a timer callback that runs periodically.
 */
void EventLoop::run() {
    this->activate();

    event_base_dispatch(this->evbase);
}

/**
 * @brief Handle a signal that indicates the process should terminate
 */
void EventLoop::handleTermination() {
    PLOG_WARNING << "Received signal, terminating...";

    // break out of the main loop and initiate shutdown
    gRun = false;
    event_base_loopbreak(this->evbase);
}
