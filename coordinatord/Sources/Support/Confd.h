#ifndef SUPPORT_CONFD_H
#define SUPPORT_CONFD_H

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace Support {
/**
 * @brief Interface to confd
 *
 * Provides a more convenient interface to the configuration daemon (confd) used for various
 * runtime configuration data.
 */
class Confd {
    public:
        static void Init();

        static std::optional<int64_t> GetInteger(const std::string_view &key);
        static std::optional<double> GetReal(const std::string_view &key);
        static size_t GetBlob(const std::string_view key, std::span<std::byte> outBuffer);

    private:
        Confd() = delete;

        static void EnsureSuccess(const int, const std::string_view &);
};
}

#endif
