#include <getopt.h>
#include <unistd.h>
#include <event2/event.h>

#include <atomic>
#include <filesystem>
#include <iostream>
#include <string>

#include "version.h"
#include "misc/Logging.h"

/// Set for as long as we should continue processing requests
std::atomic_bool gRun{true};


/**
 * @brief Daemon entry point
 */
int main(int argc, char **argv) {
    std::string confPath;
    int logLevel{0};
    bool logSimple{false};

    // parse the command line
    int c;
    while(1) {
        int index{0};
        const static struct option options[] = {
            // config file
            {"config",                  required_argument, 0, 0},
            // log severity
            {"log-level",               optional_argument, 0, 0},
            // log style (simple = no timestamps, for systemd/syslog use)
            {"log-simple",              no_argument, 0, 0},
            {nullptr,                   0, 0, 0},
        };

        c = getopt_long_only(argc, argv, "", options, &index);

        // end of options
        if(c == -1) {
            break;
        }
        // long option (based on index)
        else if(!c) {
            if(index == 0) {
                confPath = optarg;
            }
            // log verbosity (centered around warning level)
            else if(index == 1) {
                logLevel = strtol(optarg, nullptr, 10);
            }
            // use simple log format
            else if(index == 2) {
                logSimple = true;
            }
        }
    }

    if(confPath.empty()) {
        std::cerr << "you must specify a config file (--config)" << std::endl;
        return -1;
    }

    // do set up
    Support::InitLogging(logLevel, logSimple);
    PLOG_INFO << "blazed version " << kVersion << " (" << kVersionGitHash << ")";

    // run the event loop on the main thread
    while(gRun) {
        // TODO: implement
    }

    // clean up
    PLOG_DEBUG << "shutting down";
}
