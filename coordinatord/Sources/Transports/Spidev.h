#ifndef TRANSPORTS_SPIDEV_H
#define TRANSPORTS_SPIDEV_H

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <toml++/toml.h>

#include "Transports/Commands.h"
#include "Transports/Base.h"

namespace Transports {
/**
 * @brief SPI radio transport
 *
 * Radio transport for a radio connected via an SPI interface. An interrupt line is required, with
 * an optional reset line.
 */
class Spidev: public TransportBase {
    private:
        /**
         * @brief Read command delay (µS)
         *
         * This is the time period that we'll delay after transmitting the command header, but
         * before receiving the response for a "read" command.
         */
        constexpr static const size_t kReadCmdDelay{25};

        /**
         * @brief Write command delay (µS)
         *
         * This is the time period that we'll delay after transmitting the command header, but
         * before sending the payload for "write" commands.
         */
        constexpr static const size_t kWriteCmdDelay{10};

        /**
         * @brief Post-command delay (µS)
         *
         * A delay period that's inserted after all commands, to allow for turnaround time in the
         * software.
         */
        constexpr static const size_t kPostCmdDelay{25};

    public:
        Spidev(const toml::table &config);
        ~Spidev();

        void reset() override;

        void sendCommandWithResponse(const CommandId command, std::span<std::byte> buffer) override;
        void sendCommandWithPayload(const CommandId command,
                std::span<const std::byte> payload) override;

    private:
        void openSpidev(const toml::table &);
        void initIrq(const std::string &);
        void initReset(const std::string &);

        void handleIrq(int, size_t);

        static std::pair<std::string, size_t> ParseGpio(const std::string &);

    private:
        /// SPI device file descriptor
        int spidev{-1};

        /// IRQ line
        struct gpiod_line *irqLine{nullptr};
        /// Event to observe the IRQ line fd
        struct event *irqLineEvent{nullptr};

        /// Reset line
        struct gpiod_line *resetLine{nullptr};
};
}

#endif
