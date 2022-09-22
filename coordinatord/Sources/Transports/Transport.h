#ifndef TRANSPORTS_TRANSPORT_H
#define TRANSPORTS_TRANSPORT_H

#include <cstddef>
#include <cstdint>
#include <span>

/**
 * @brief Abstract base class for radio transport
 *
 * Transports provide a relatively low-level interface to send and receive binary commands. The
 * commands are passed into the transport using the high level interface, which is then responsible
 * for applying transport-specific framing.
 */
class Transport {
    public:
        virtual ~Transport() = default;

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
        virtual void sendCommandWithResponse(const uint8_t command,
                std::span<uint8_t> buffer) = 0;

        /**
         * @brief Send a command with payload
         *
         * @param command Command id
         * @param payload Data payload to send with the command
         */
        virtual void sendCommandWithPayload(const uint8_t command,
                std::span<const uint8_t> payload) = 0;
};

#endif
