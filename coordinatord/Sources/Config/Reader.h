#ifndef CONFIG_READER_H
#define CONFIG_READER_H

#include <filesystem>

namespace Config {
void Read(const std::filesystem::path &configFile);
};

#endif
