#include <getopt.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

#include <event2/event.h>
#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/FuncMessageFormatter.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>

#include "misc/Logging.h"

using namespace Support;

/**
 * @brief Initialize logging
 *
 * Sets up the plog logging framework. We redirect all log output to stderr, under the assumption
 * that we'll be running under some sort of supervisor that handles capturing and storing
 * these messages.
 *
 * @param level Minimum log level to output
 * @param simple Whether the simple message output format (no timestamps) is used
 */
static void InitPlog(const plog::Severity level, const bool simple) {
    // figure out if the console is a tty
    const bool isTty = (isatty(fileno(stdout)) == 1);

    // set up the logger
    if(simple) {
        if(isTty) {
            static plog::ColorConsoleAppender<plog::FuncMessageFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        } else {
            static plog::ConsoleAppender<plog::FuncMessageFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        }
    } else {
        if(isTty) {
            static plog::ColorConsoleAppender<plog::TxtFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        } else {
            static plog::ConsoleAppender<plog::TxtFormatter> ttyAppender;
            plog::init(level, &ttyAppender);
        }
    }
}

/**
 * @brief Initialize libevent
 *
 * Configure the log callback for libevent to use our existing logging machinery.
 */
static void InitLibevent() {
    event_set_log_callback([](const auto severity, const auto msg) {
        switch(severity) {
            case EVENT_LOG_DEBUG:
                PLOG_DEBUG << msg;
                break;
            case EVENT_LOG_MSG:
                PLOG_INFO << msg;
                break;
            case EVENT_LOG_WARN:
                PLOG_WARNING << msg;
                break;
            default:
                PLOG_ERROR << msg;
                break;
        }
    });
}

/**
 * @brief Initialize the logging system
 *
 * Set up plog. This will parse the command line specified (if non-NULL) and extract the
 * `log-level` and `log-simple` keys.
 *
 * @param level What level messages to output ([-3, 2]) where 2 is the most
 * @param simple When set, no timestamp/function name info is printed
 */
void Support::InitLogging(const int level, const bool simple) {
    plog::Severity logLevel{plog::Severity::info};

    switch(level) {
        case -3:
            logLevel = plog::Severity::fatal;
            break;
        case -2:
            logLevel = plog::Severity::error;
            break;
        case -1:
            logLevel = plog::Severity::warning;
            break;
        case 0:
            logLevel = plog::Severity::info;
            break;
        case 1:
            logLevel = plog::Severity::debug;
            break;
        case 2:
            logLevel = plog::Severity::verbose;
            break;

        default:
            throw std::runtime_error("invalid log level: must be [-3, 2]");
    }

    InitPlog(logLevel, simple);
    InitLibevent();
}
