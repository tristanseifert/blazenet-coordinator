#ifndef LOG_LOGGER_H
#define LOG_LOGGER_H

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <etl/span.h>
#include <etl/string_view.h>

#include "Rtos/Rtos.h"

extern "C" void log_panic(const char *, ...);

/// Log message handling
namespace Log {
/**
 * @brief Global logging handler
 *
 * The logger is a global object capable of formatting messages, at a given intensity level, and
 * writing them to multiple output destinations. Logs may also be archived on some form of
 * persistent storage for later retrieval.
 *
 * Messages can be output either via the debugger's trace facilities (for when things are really
 * fucked) or to the host via the DMA driven TTY UART, running at 4Mbaud.
 */
class Logger {
    friend void ::log_panic(const char *, ...);

    public:
        /**
         * @brief Size of a per task log buffer (in bytes)
         *
         * This sets an upper cap on the maximum length of a single log message.
         */
        constexpr static const size_t kTaskLogBufferSize{256};

        /**
         * @brief Whether log messages are output via debug trace SWO
         */
        constexpr static const bool kEnableTraceSwo{false};

        /**
         * @brief Whether log messages are output via UART
         *
         * This serial port runs at 921600 baud, and is connected directly to the host. All
         * messages received are timestamped and stored in the system log.
         */
        constexpr static const bool kEnableUartTty{true};

    private:
        static void TracePutString(etl::span<const char> str);

    public:
        /**
         * @brief Log level
         *
         * An enumeration defining the different log levels (intensities) available. Messages with
         * a level below the cutoff may be filtered out and discarded.
         */
        enum class Level: uint8_t {
            /// Most severe type of error
            Error                       = 5,
            /// A significant problem in the system
            Warning                     = 4,
            /// General information
            Notice                      = 3,
            /// Bonus debugging information
            Debug                       = 2,
            /// Even more verbose debugging information
            Trace                       = 1,
        };

    public:
        static void Init();

        /// You cannot create logger instances; there is just one shared boi
        Logger() = delete;

        /**
         * @brief Panic the system
         *
         * This is the same as logging an error and then invoking halt callback.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        [[noreturn]] static void Panic(const etl::string_view fmt, ...) {
            va_list va;
            va_start(va, fmt);
            Log(Level::Error, fmt, va);
            va_end(va);

            Panic();
        }

        /**
         * @brief Output an error level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Error(const etl::string_view fmt, ...) {
            va_list va;
            va_start(va, fmt);
            Log(Level::Error, fmt, va);
            va_end(va);
        }

        /**
         * @brief Output a warning level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Warning(const etl::string_view fmt, ...) {
            if(static_cast<uint8_t>(gLevel) > static_cast<uint8_t>(Level::Warning)) return;

            va_list va;
            va_start(va, fmt);
            Log(Level::Warning, fmt, va);
            va_end(va);
        }

        /**
         * @brief Output a notice level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Notice(const etl::string_view fmt, ...) {
            if(static_cast<uint8_t>(gLevel) > static_cast<uint8_t>(Level::Notice)) return;

            va_list va;
            va_start(va, fmt);
            Log(Level::Notice, fmt, va);
            va_end(va);
        }

        /**
         * @brief Output a debug level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Debug(const etl::string_view fmt, ...) {
            if(static_cast<uint8_t>(gLevel) > static_cast<uint8_t>(Level::Debug)) return;

            va_list va;
            va_start(va, fmt);
            Log(Level::Debug, fmt, va);
            va_end(va);
        }

        /**
         * @brief Output a trace level message.
         *
         * @param fmt Format string
         * @param ... Arguments to message
         */
        static void Trace(const etl::string_view fmt, ...) {
            if(static_cast<uint8_t>(gLevel) > static_cast<uint8_t>(Level::Trace)) return;

            va_list va;
            va_start(va, fmt);
            Log(Level::Trace, fmt, va);
            va_end(va);
        }

        static void Log(const Level lvl, const etl::string_view &fmt, va_list args);

    private:
        [[noreturn]] static void Panic();

    private:
        static bool gInitialized;
        static Level gLevel;

        static SemaphoreHandle_t gUartCompletion;
};
}

using Logger = Log::Logger;

#define REQUIRE(cond, ...) {if(!(cond)) { Logger::Panic(__VA_ARGS__); }}

#endif
