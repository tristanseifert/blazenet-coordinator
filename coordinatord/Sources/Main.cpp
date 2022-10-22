#include <getopt.h>
#include <unistd.h>
#include <event2/event.h>

#include <TristLib/Core.h>
#include <TristLib/Event.h>

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
#include "Rpc/Server.h"
#include "Support/Confd.h"
#include "Transports/Base.h"

/// Set for as long as we should continue processing requests
std::atomic_bool gRun{true};
/// Main run loop
static std::shared_ptr<TristLib::Event::RunLoop> gMainLoop;
/// Job supervisor watchdog
static std::shared_ptr<TristLib::Event::SystemWatchdog> gWdog;
/// Ctrl+C handler
static std::shared_ptr<TristLib::Event::Signal> gSignalHandler;

/// Packet handler
static std::shared_ptr<Protocol::Handler> gHandler;
/// Local RPC server
static std::shared_ptr<Rpc::Server> gLocalRpc;

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
 * @brief Initialize the run loop
 */
static void InitRunLoop() {
    // create the loop
    gMainLoop = std::make_shared<TristLib::Event::RunLoop>();
    gMainLoop->arm();

    // set up signal handler
    gSignalHandler = std::make_shared<TristLib::Event::Signal>(gMainLoop,
            TristLib::Event::Signal::kQuitEvents, [](auto) {
        PLOG_WARNING << "Received signal, terminating…";
        gRun = false;
        gMainLoop->interrupt();
    });

    // set up system watchdog
    gWdog = std::make_shared<TristLib::Event::SystemWatchdog>(gMainLoop);
}

/**
 * @brief Run the deamon's main loop
 */
static void RunMainLoop() {
    gWdog->start();

    while(gRun) {
        gMainLoop->run();
    }

    gWdog->stop();
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

    TristLib::Core::InitLogging(gCliConfig.logLevel, gCliConfig.logShortFormat);
    PLOG_INFO << "Starting blazed version " << kVersion << " (" << kVersionGitHash << ")";

    // set up run loop
    InitRunLoop();

    // read config file and set up confd
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

        // set up protocol handler
        gHandler = std::make_shared<Protocol::Handler>(radio);

        // lastly, set up RPC server
        gLocalRpc = std::make_shared<Rpc::Server>(radio, gHandler);
    } catch(const std::exception &e) {
        PLOG_FATAL << "Initialization failed: " << e.what();
        return 1;
    }

    // run the event loop on the main thread
    RunMainLoop();

    // clean up
    PLOG_DEBUG << "Shutting down…";

    gLocalRpc.reset();
    gHandler.reset();
    gMainLoop.reset();

    return 0;
}
