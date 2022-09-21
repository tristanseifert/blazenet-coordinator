#include "em_device.h"
#include "em_cmu.h"
#include "em_gpio.h"
#include "sl_debug_swo.h"

#include "Logger.h"

#include "Drivers/sl_uartdrv_instances.h"
#include "Rtos/Rtos.h"

#include <printf/printf.h>
#include <stdarg.h>
#include <stdlib.h>
#include <etl/array.h>

using namespace Log;

/**
 * @brief Indicates whether the logger backends have been initialized
 *
 * During early system boot, we won't have any output backends for log messages set up; in this
 * case, fall back to just printing them out the swd trace line.
 */
bool Logger::gInitialized{false};

/**
 * @brief Log level
 *
 * All log messages with a level that is numerically lower than this one will be discarded.
 */
Logger::Level Logger::gLevel{Logger::Level::Trace};

/**
 * @brief UART transmit completion flag
 */
SemaphoreHandle_t Logger::gUartCompletion{nullptr};


/**
 * @brief Initialize the logger instance
 *
 * This sets up the output drivers.
 */
void Logger::Init() {
    // set up trace SWO
    if(kEnableTraceSwo) {
        CMU_ClockDivSet(cmuClock_TRACECLK, 2);

        sl_debug_swo_init();
        sl_debug_swo_enable_itm(0);
    }

    // set up signalling for UART
    if(kEnableUartTty) {
        static StaticSemaphore_t gStorage;

        gUartCompletion = xSemaphoreCreateBinaryStatic(&gStorage);
        REQUIRE(!!gUartCompletion, "failed to initialize %s", "UART completion");

        xSemaphoreGive(gUartCompletion);
    }
}



/**
 * @brief Output a log message.
 *
 * @param level Message level
 * @param format Format string, with printf-style substitutions
 * @param args Arguments to format
 *
 * This formats the message into an intermediate task specific buffer; this avoids needing to take
 * a lock during this process, only when we actually modify the trace buffer do we do so from
 * inside a critical section.
 *
 * @remark Do not add a trailing newline on the message's format string. This is automatically
 *         added, if necessary, to signify the end of a message by the underlying output drivers.
 */
void Logger::Log(const Level level, const etl::string_view &format, va_list args) {
    int numChars{0};
    size_t bufferSz{0}, bytesWritten{0};
    char *buffer{nullptr}, *bufferStart{nullptr};
    bool hasScheduler{true};

    // log buffer used before scheduler is started
    static char gLogBuffer[kTaskLogBufferSize];
    // flag set when the initial log buffer was assigned to a task
    static bool gLogBufferAssigned{false};

    /*
     * Get the task-specific log buffer (and its length) if the scheduler has been started;
     * otherwise use a statically allocated buffer (which is then re-used for the first task)
     */
    if(xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        buffer = gLogBuffer;
        bufferSz = kTaskLogBufferSize;
        hasScheduler = false;
    }
    // scheduler is running, so query thread-local storage
    else {
        auto ptr = pvTaskGetThreadLocalStoragePointer(nullptr,
                Rtos::ThreadLocalIndex::TLSLogBuffer);

        if(!ptr) {
            // if old buffer hasn't been assigned, just use that
            if(!__atomic_test_and_set(&gLogBufferAssigned, __ATOMIC_RELAXED)) {
                buffer = gLogBuffer;
            }
            // otherwise we need to allocate a buffer
            else {
                buffer = static_cast<char *>(malloc(kTaskLogBufferSize));
                REQUIRE(buffer, "failed to allocate log buffer");
            }

            // store the used buffer in TLS after zeroing it
            memset(buffer, 0, bufferSz);
            vTaskSetThreadLocalStoragePointer(nullptr, Rtos::ThreadLocalIndex::TLSLogBuffer,
                    buffer);
        } else {
            buffer = static_cast<char *>(ptr);
        }

        // buffer is always this same size
        bufferSz = kTaskLogBufferSize;

        // acquire UART output
        // XXX: could be skipped if we're outputting a different buffer right now
        xSemaphoreTake(gUartCompletion, portMAX_DELAY);
    }

    bufferStart = buffer;

    // output a timestamp (TODO: use something with better resolution than ticks?)
    const auto ticks = xTaskGetTickCount();
    numChars = snprintf(buffer, bufferSz, "[%10u] ", ticks);

    bytesWritten += numChars;
    bufferSz -= numChars;
    buffer += numChars;

    // format the message
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
    numChars = vsnprintf(buffer, bufferSz, format.data(), args);
#pragma clang diagnostic pop

    if(numChars >= bufferSz) {
        numChars = (bufferSz - 1);
    }

    bytesWritten += numChars;
    bufferSz -= numChars;
    buffer += numChars;

    *buffer++ = '\n';
    bytesWritten++;

    // write it to our output devices
    //taskENTER_CRITICAL();
    if(kEnableTraceSwo) {
        TracePutString({bufferStart, bytesWritten});
    }
    if(kEnableUartTty) {
        // TODO: transmit timestamp, severity as binary
        if(hasScheduler) {
            // use DMA driven transmission here
            UARTDRV_Transmit(sl_uartdrv_eusart_tty_handle, reinterpret_cast<uint8_t *>(bufferStart),
                    bytesWritten, [](auto handle, auto status, auto data, auto dataLen) {
                BaseType_t woken{pdFALSE};
                xSemaphoreGiveFromISR(gUartCompletion, &woken);
                portYIELD_FROM_ISR(woken);
            });
        } else {
            // scheduler isn't running, so write it out directly
            UARTDRV_ForceTransmit(sl_uartdrv_eusart_tty_handle, reinterpret_cast<uint8_t *>(bufferStart),
                    bytesWritten);
        }
    }
    //taskEXIT_CRITICAL();
}

/**
 * @brief Panic the system
 *
 * This disables interrupts and lands ourselves into an infinite loop and/or breakpoint.
 */
void Logger::Panic() {
    // print a message
    Error("Panic! at the system, halting");
    // Hw::StatusLed::Set(Hw::StatusLed::Color::Red);

    // get task info (if scheduler is running)
    unsigned long totalRuntime{0};
    constexpr static const size_t kTaskInfoSize{8};
    static etl::array<TaskStatus_t, kTaskInfoSize> gTaskInfo;

    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        //const auto ok = uxTaskGetSystemState(gTaskInfo.data(), kTaskInfoSize, &totalRuntime);
        const auto ok = uxTaskGetSystemState(gTaskInfo.data(), kTaskInfoSize, nullptr);

        if(!ok) {
            Error("Failed to get RTOS state");
        } else {
            Error("========== RTOS state ==========");
            Error("Total runtime: %10lu", totalRuntime);
            Error("%8s %-16s S %10s %3s %3s", "Handle", "Name", "Runtime", "PRI", "STK");

            for(size_t i = 0; i < ok; i++) {
                const auto &task = gTaskInfo[i];
                char stateChar{'?'};

                switch(task.eCurrentState) {
                    case eReady:
                        stateChar = 'R';
                        break;
                    case eRunning:
                        stateChar = '*';
                        break;
                    case eBlocked:
                        stateChar = 'B';
                        break;
                    case eSuspended:
                        stateChar = 'S';
                        break;
                    case eDeleted:
                        stateChar = 'x';
                        break;
                    default:
                        break;
                }

                Error("%08x %-16s %c %10lu %3u %03x", task.xHandle, task.pcTaskName, stateChar,
                        task.ulRunTimeCounter, task.uxCurrentPriority, task.usStackHighWaterMark);
            }
        }
    }

    // stop machine
    __disable_irq();
    __BKPT(0xf3);

    while(1) {}
}

/**
 * @brief C panic function
 *
 * Mimics a real panic from a C only source file (such as FreeRTOS)
 *
 * @param fmt Format string
 * @param ... Arguments to format (in format string)
 */
extern "C" void log_panic(const char *fmt, ...) {
    using Level = Log::Logger::Level;

    va_list va;
    va_start(va, fmt);
    Log::Logger::Log(Level::Error, fmt, va);
    va_end(va);

    Logger::Panic();
}

/**
 * @brief C logging thunk
 *
 * Calls through to the actual logging implementation from C.
 *
 * @param inLevel Message level to output at; equal to one of the constant values of Level enum
 * @param fmt Format string
 * @param ... Arguments to format (in format string)
 */
extern "C" void do_log(const unsigned int inLevel, const char *fmt, ...) {
    using Level = Log::Logger::Level;

    va_list va;
    va_start(va, fmt);

    // validate priority
    Level lvl;
    switch(inLevel) {
        case 5:
            lvl = Level::Error;
            break;
        case 4:
            lvl = Level::Warning;
            break;
        case 3:
            lvl = Level::Notice;
            break;
        case 2:
            lvl = Level::Debug;
            break;
        case 1:
            lvl = Level::Trace;
            break;

        // unknown log level
        default:
            Logger::Error("Invalid log level: %u", inLevel);
            return;
    }

    // perform logging
    Log::Logger::Log(lvl, fmt, va);
    va_end(va);
}



/**
 * @brief Write the provided message to the trace output
 *
 * @remark All messages will be terminated with a newline automatically.
 */
void Logger::TracePutString(etl::span<const char> str) {
    for(size_t i = 0; i < str.size(); i++) {
        sl_debug_swo_write_u8(0, str[i]);
    }

    sl_debug_swo_write_u8(0, '\n');
}
