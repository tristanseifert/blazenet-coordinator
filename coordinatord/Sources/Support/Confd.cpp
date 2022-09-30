#ifndef CONFIG_WITH_CONFD
#error "Confd support requires the confd runtime library!"
#endif

#include <stdexcept>

#include <confd/confd.h>
#include <fmt/format.h>

#include "Support/Confd.h"

using namespace Support;

/**
 * @brief Initialize confd connection
 *
 * This will initialize the confd runtime library, which internally opens a socket to the daemon.
 */
void Confd::Init() {
    // TODO: use socket path from config file, if specified
    auto err = confd_open(nullptr);
    EnsureSuccess(err, "confd_open");
}

/**
 * @brief Ensure a confd call completed successfully
 */
void Confd::EnsureSuccess(const int error, const std::string_view &what) {
    if(error != kConfdStatusSuccess) {
        throw std::runtime_error(fmt::format("{} (confd error {}: {})", what,
                    error, confd_strerror(error)));
    }
}



/**
 * @brief Read a key as an integer
 *
 * @param key Key name to look up
 *
 * @return Value of the key, if any; a nonexistent key is returned as an empty optional.
 */
std::optional<int64_t> Confd::GetInteger(const std::string_view &key) {
    int64_t temp;
    auto err = confd_get_int(key.data(), &temp);
    if(err == kConfdNotFound) {
        return std::nullopt;
    } else if(err == kConfdNullValue) {
        return std::nullopt;
    }

    EnsureSuccess(err, fmt::format("read int {}", key));

    return temp;
}

/**
 * @brief Read a key as a real number
 *
 * @param key Key name to look up
 *
 * @return Value of the key, if any; a nonexistent key is returned as an empty optional.
 */
std::optional<double> Confd::GetReal(const std::string_view &key) {
    double temp;
    auto err = confd_get_real(key.data(), &temp);
    if(err == kConfdNotFound) {
        return std::nullopt;
    } else if(err == kConfdNullValue) {
        return std::nullopt;
    }

    EnsureSuccess(err, fmt::format("read real {}", key));

    return temp;
}
