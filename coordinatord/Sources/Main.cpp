#include <getopt.h>
#include <unistd.h>
#include <event2/event.h>

#include <atomic>
#include <filesystem>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Radio.h"
#include "version.h"
#include "Config/Reader.h"
#include "Protocol/Handler.h"
#include "Transports/Base.h"
#include "Support/Confd.h"
#include "Support/EventLoop.h"
#include "Support/Logging.h"
#include "Support/Watchdog.h"

/// Set for as long as we should continue processing requests
std::atomic_bool gRun{true};
/// Main run loop
static std::shared_ptr<Support::EventLoop> gMainLoop;
/// Packet handler
static std::shared_ptr<Protocol::Handler> gHandler;

/// Command line configuration
static struct {
    /// Path to the daemon config file
    std::filesystem::path configFilePath;

    /// Logging severity ([-3, 2] where 2 shows the most messages and -3 the least)
    int logLevel{0};
    /// Use the short log format (omit timestamps)
    bool logShortFormat{false};
} gCliConfig;



/**
 * @brief Parse the command line
 *
 * @throw std::runtime_error When an option is incorrect
 */
static void ParseArgs(const int argc, char **argv) {
    int c;

    const static struct option options[] = {
        // config file
        {"config",                  required_argument, 0, 0},
        // log severity
        {"log-level",               optional_argument, 0, 0},
        // log style (simple = no timestamps, for systemd/syslog use)
        {"log-simple",              no_argument, 0, 0},
        {nullptr,                   0, 0, 0},
    };

    // loop as long as there's options to process
    while(1) {
        int index{0};
        c = getopt_long_only(argc, argv, "", options, &index);

        // end of options
        if(c == -1) {
            break;
        }
        // long option (based on index)
        else if(!c) {
            if(index == 0) {
                gCliConfig.configFilePath = optarg;
            }
            // log verbosity (centered around warning level)
            else if(index == 1) {
                gCliConfig.logLevel = strtol(optarg, nullptr, 10);
            }
            // use simple log format
            else if(index == 2) {
                gCliConfig.logShortFormat = true;
            }
        }
    }

    // validate the parsed options
    if(gCliConfig.configFilePath.empty()) {
        throw std::runtime_error("You must specify a config file (--config)");
    }
    else if(gCliConfig.logLevel < -3 || gCliConfig.logLevel > 2) {
        throw std::runtime_error("Invalid log level (must be [-3, 2])");
    }
}

/**
 * @brief Run the deamon's main loop
 */
static void RunMainLoop() {
    Support::Watchdog::Start();

    while(gRun) {
        gMainLoop->run();
    }

    Support::Watchdog::Stop();
}

/**
 * @brief Daemon entry point
 */
int main(int argc, char **argv) {
    // early init: parse args and set up logging
    try {
        ParseArgs(argc, argv);
    } catch(const std::exception &e) {
        std::cerr << "Failed to parse arguments: " << e.what() << std::endl;
        return 1;
    }

    Support::InitLogging(gCliConfig.logLevel, gCliConfig.logShortFormat);
    PLOG_INFO << "Starting blazed version " << kVersion << " (" << kVersionGitHash << ")";

    // initialize the event loop, then do config initialization
    gMainLoop = std::make_shared<Support::EventLoop>(true);
    gMainLoop->arm();

    try {
        Config::Read(gCliConfig.configFilePath);
    } catch(const std::exception &e) {
        PLOG_FATAL << "Failed to parse config file: " << e.what();
        return 1;
    }
    try {
        Support::Confd::Init();
    } catch(const std::exception &e) {
        PLOG_FATAL << "Failed to set up runtime config support: " << e.what();
        return 1;
    }

    // perform more initialization
    try {
        // set up the radio transport and radio instance (which will configure the radio)
        auto transport = Transports::TransportBase::Make(Config::GetTransportConfig());
        if(!transport) {
            throw std::runtime_error("failed to initialize transport (check transport type)");
        }

        auto radio = std::make_shared<Radio>(transport);

        // set up packet handler
        gHandler = std::make_shared<Protocol::Handler>(radio);
    } catch(const std::exception &e) {
        PLOG_FATAL << "Initialization failed: " << e.what();
        return 1;
    }

    // run the event loop on the main thread
    RunMainLoop();

    // clean up
    PLOG_DEBUG << "Shutting down…";

    gHandler.reset();

    gMainLoop.reset();
}
