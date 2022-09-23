#ifndef CONFIG_READER_H
#define CONFIG_READER_H

#include <filesystem>
#include <toml++/toml.h>

namespace Config {
void Read(const std::filesystem::path &configFile);

const toml::table &GetTransportConfig();
};

#endif
