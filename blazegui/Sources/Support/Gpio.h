#ifndef SUPPORT_GPIO_H
#define SUPPORT_GPIO_H

#include <fmt/format.h>

#include <gpiod.h>

#include <cerrno>
#include <cstddef>
#include <regex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace Support {
class Gpio {
    private:
        /**
         * @brief Parse a GPIO descriptor
         *
         * This is a string in the format of `gpiochip:pin`, where `pin` is an integer.
         *
         * @param in Input string to parse
         * @return A pair of GPIP chip name and pin number
         */
        static std::pair<std::string, size_t> Parse(const std::string &in) {
            static const std::regex kRegex("(\\w+)(?::)(\\d+)");
            std::smatch match;

            if(!std::regex_match(in, match, kRegex)) {
                throw std::runtime_error(fmt::format("invalid gpio descriptor: `{}`", in));
            }

            return std::make_pair(match[1].str(), std::stol(match[2].str()));
        }

    public:
        /**
         * @brief Get a GPIO line
         *
         * Parse the GPIO descriptor, and return the corresponding GPIO handle.
         *
         * @return GPIO handle, or `nullptr`
         */
        static struct gpiod_line *GetLine(const std::string &name) {
            const auto [chipName, pin] = Parse(name);

            // get the IO line
            auto line = gpiod_line_get(chipName.c_str(), pin);
            if(!line) {
                throw std::system_error(errno, std::generic_category(),
                        fmt::format("failed to get irq ({})", name));
            }

            return line;
        }

        /**
         * @brief Set the state of an IO line
         *
         * @param line Line to change the state of
         * @param state New state to apply to the line
         * @param desc Optional description for this IO line operation (for error messages)
         *
         * @throw std::system_error If IO operation failed
         */
        static void SetState(struct gpiod_line *line, const int state,
                const char *desc = "change gpio state") {
            int err = gpiod_line_set_value(line, state);
            if(err) {
                throw std::system_error(errno, std::generic_category(), desc);
            }
        }
};
}

#endif
