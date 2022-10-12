#include <fmt/format.h>

#include <stdexcept>

#include "Config/Reader.h"
#include "Support/Logging.h"

#include "Drivers/Display/Base.h"
#include "Drivers/Display/St7789.h"

#include "Init.h"

using namespace Drivers;

static std::shared_ptr<Display::Base> gDisplayDriver;



/**
 * @brief Initialize display driver
 *
 * @param conf Display configuration table
 *
 * @return An initialized display driver instance
 */
static std::shared_ptr<Display::Base> InitDisplayDriver(const toml::table &conf) {
    if(!conf.is_table()) {
        throw std::invalid_argument("invalid display config (expected table)");
    }

    const std::string type = conf.at_path("driver").value_or("(null)");

    if(type == "st7789") {
        return std::make_shared<Display::St7789>(conf);
    }
    // unknown type of display driver
    else {
        throw std::runtime_error(fmt::format("Unsupported display driver `{}`", type));
    }
}

/**
 * @brief Initialize LED driver
 *
 * @param conf TOML node representing the LED
 *
 * @return An initialized LED driver instance
 */
static void *InitLedDriver(const toml::table &conf) {
    if(!conf.is_table()) {
        throw std::invalid_argument("invalid led config (expected table)");
    }

    // get the type
    const std::string type = conf.at_path("type").value_or("(null)");

    // initialize the driver based on type
    if(type == "rgb") {
        // TODO: implement
    } else {
        throw std::runtime_error(fmt::format("Unsupported status led type `{}`", type));
    }
}



/**
 * @brief Load and initialize drivers
 *
 * Determine what drivers should be initialized for each of the display, buttons, and LEDs, if any
 * based on the configuration.
 */
void Drivers::Init() {
    const auto &conf = Config::GetConfig();

    // display: based on type
    auto displayInfo = conf.at_path("display");
    if(displayInfo && displayInfo.is_table()) {
        gDisplayDriver = InitDisplayDriver(*displayInfo.as_table());
    }
    else if(displayInfo) {
        throw std::runtime_error("invalid `display` key (expected table)");
    } else {
        PLOG_INFO << "No displays defined; will not provide GUI support";
    }

    // buttons: always event type
    // TODO: implement

    // LED: only the status led is supported so far
    auto status = conf.at_path("led.status");
    if(status && status.is_table()) {
        auto statusLed = InitLedDriver(*status.as_table());
    } else if(status) {
        throw std::runtime_error("invalid `led.status` key");
    }
}

/**
 * @brief Clean up driver support
 *
 * Shut down all currently initialized drivers.
 */
void Drivers::CleanUp() {
    gDisplayDriver.reset();
}



/**
 * @brief Get the current display driver (if any)
 */
std::shared_ptr<Display::Base> &Drivers::GetDisplayDriver() {
    return gDisplayDriver;
}
