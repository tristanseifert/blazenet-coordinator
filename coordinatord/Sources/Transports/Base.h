#ifndef TRANSPORTS_BASE_H
#define TRANSPORTS_BASE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>
#include <toml++/toml.h>

#include "Commands.h"

namespace Transports {
/**
 * @brief Abstract base class for radio transport
 *
 * Transports provide a relatively low-level interface to send and receive binary commands. The
 * commands are passed into the transport using the high level interface, which is then responsible
 * for applying transport-specific framing.
 */
class TransportBase {
    public:
        static std::shared_ptr<TransportBase> Make(const toml::table &root);

    public:
        virtual ~TransportBase() = default;

        /**
         * @brief Reset the radio
         */
        virtual void reset() = 0;

        /**
         * @brief Send a command, then read response
         *
         * Send a command structure to the radio, and read a response of the specified size.
         *
         * @param command Command id
         * @param buffer Buffer to receive the command response
         */
        virtual void sendCommandWithResponse(const CommandId command,
                std::span<std::byte> buffer) = 0;

        /**
         * @brief Send a command with payload
         *
         * @param command Command id
         * @param payload Data payload to send with the command
         */
        virtual void sendCommandWithPayload(const CommandId command,
                std::span<const std::byte> payload) = 0;

        /**
         * @brief Register an interrupt handler
         *
         * @param handler Function to invoke when an interrupt is detected
         */
        virtual void addIrqHandler(const std::function<void()> &handler) {
            irqHandlers.emplace_back(handler);
        }

    protected:
        /**
         * @brief Invoke all registered interrupt handlers
         */
        virtual void invokeIrqHandlers() {
            for(const auto &handler : this->irqHandlers) {
                handler();
            }
        }

    protected:
        /// Registered interrupt handlers
        std::vector<std::function<void()>> irqHandlers;
};
}

#endif
