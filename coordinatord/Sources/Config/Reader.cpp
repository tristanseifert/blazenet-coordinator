#include <fmt/format.h>
#include <toml++/toml.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include "Support/Logging.h"
#include "Config/Reader.h"

#ifdef WITH_TRANSPORT_SPIDEV
#include "Transports/Spidev.h"
#endif

using namespace Config;

static void ReadConfd(const toml::table &);
static void ReadRadio(const toml::table &);
static void ReadRadioTransport(const toml::table &);
static void ReadRadioRegion(const toml::table &);

/**
 * @brief Read configuration file from disk
 *
 * Open the TOML-formatted configuration file from the disk, then read out the individual sections
 * of configuration into memory.
 *
 * @param configFile Path to the configuration file on disk
 *
 * @remark All errors with the configuration (IO errors, TOML parsing errors, and logical errors
 *         such as invalid values) are surfaced as exceptions from here.
 */
void Config::Read(const std::filesystem::path &configFile) {
    toml::table root;

    // Open TOML file and propagate parse errors with more info
    try {
        root = toml::parse_file(configFile.native());
    } catch(const toml::parse_error &err) {
        const auto &beg = err.source().begin;
        throw std::runtime_error(fmt::format("At line {}, column {}: {}", beg.line, beg.column,
                    err.description()));
    }

    // read confd connection
    const auto confd = root["confd"];
    if(confd) {
        if(!confd.is_table()) {
            throw std::runtime_error("invalid `confd` key (expected table)");
        }

        ReadConfd(*confd.as_table());
    }

    // read radio configuration
    const auto radio = root["radio"];
    if(radio) {
        if(!radio.is_table()) {
            throw std::runtime_error("invalid `radio` key (expected table)");
        }

        ReadRadio(*radio.as_table());
    } else {
        throw std::runtime_error("missing `radio` key");
    }
}

/**
 * @brief Read the confd section
 *
 * This section has the following keys:
 *
 * - socket: Path to the RPC socket (if any)
 */
static void ReadConfd(const toml::table &root) {
    const auto socket = root["socket"];
    if(socket && socket.is_string()) {
        // TODO: set path
    } else if(socket) {
        throw std::runtime_error("invalid `confd.socket` key (expected string)");
    }
}

/**
 * @brief Read the radio section
 *
 * This is made up of two sub-sections: `transport` and `region`.
 */
static void ReadRadio(const toml::table &root) {
    // read transport section
    const auto transport = root["transport"];
    if(transport && transport.is_table()) {
        ReadRadioTransport(*transport.as_table());
    } else if(transport) {
        throw std::runtime_error("invalid `radio.transport` key (expected table)");
    } else {
        throw std::runtime_error("missing `radio.transport` key");
    }

    // read region config
    const auto region = root["region"];
    if(region && region.is_table()) {
        ReadRadioRegion(*region.as_table());
    } else if(region) {
        throw std::runtime_error("invalid `radio.region` key (expected table)");
    } else {
        throw std::runtime_error("missing `radio.region` key");
    }
}

/**
 * @brief Read the radio transport section
 *
 * This is made up of the following mandatory keys:
 *
 * - type: Kind of transport
 *
 * Based on this type key, we'll initialize a particular transport class.
 */
static void ReadRadioTransport(const toml::table &root) {
    // get the type string
    const auto type = root["type"];
    if(!type || !type.is_string()) {
        throw std::runtime_error("missing or invalid `radio.transport.type` key");
    }

    // initialize it
    const std::string typeStr = type.value_or("");
    PLOG_VERBOSE << "Radio transport: " << typeStr;

    try {
#if WITH_TRANSPORT_SPIDEV
        if(typeStr == "spidev") {
            auto transport = std::make_shared<Transports::Spidev>(root);
            (void) transport;
            return;
        }
#endif
    } catch(const std::exception &e) {
        PLOG_FATAL << "failed to open transport: " << e.what();
        throw std::runtime_error("radio transport error");
    }

    // if we get here, no transport supported
    throw std::runtime_error(fmt::format("unknown transport `{}`", typeStr));
}

/**
 * @brief Read the radio region section
 *
 * It defines the region (and thus, frequency band) used by the radio. The following keys are
 * available:
 *
 * - country: A two character country code
 */
static void ReadRadioRegion(const toml::table &root) {
    const auto country = root["country"];
    if(!country || !country.is_string()) {
        throw std::runtime_error("missing or invalid `radio.region.country` key");
    }

    // TODO: set radio country
    const std::string countryStr = country.value_or("");
    PLOG_VERBOSE << "Radio country: " << countryStr;
}
