#include <fmt/format.h>
#include <toml++/toml.h>

#include <filesystem>
#include <stdexcept>
#include <string>

#include "Config/Reader.h"

using namespace Config;

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

    // TODO: read config sections
}
