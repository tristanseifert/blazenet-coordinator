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

/**
 * @brief Read a key as a blob
 *
 * @param key Key name to look up
 * @param outBuf Buffer to receive the key value
 *
 * @return Total number of bytes of data read, if any. A value of 0 indicates the key is empty.
 */
size_t Confd::GetBlob(const std::string_view key, std::span<std::byte> outBuffer) {
    int err;
    size_t actual{0};

    err = confd_get_blob(key.data(), outBuffer.data(), outBuffer.size(), &actual);

    // value is null = 0 bytes read out
    if(err == kConfdNullValue) {
        return 0;
    }
    // otherwise, translate it to an error (if key isn't found)
    EnsureSuccess(err, fmt::format("read blob {}", key));

    // return actual number of bytes read
    return std::min(actual, outBuffer.size());
}
