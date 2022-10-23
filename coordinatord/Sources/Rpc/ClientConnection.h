#ifndef RPC_CLIENTCONNECTION_H
#define RPC_CLIENTCONNECTION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include <TristLib/Event.h>

namespace Rpc {
class Server;

/**
 * @brief Local RPC client instance
 *
 * This class handles a single client connection, including managing the lifecycle of the socket
 * that was previously accepted.
 *
 * Client connections stay around until the RPC server is shut down, or until the underlying
 * socket is closed; they are subsequently garbage collected by the local RPC interface in a
 * periodic background task.
 */
class ClientConnection {
    private:
        /// Maximum receive packet size
        constexpr static const size_t kMaxPacketSize{4096};

        /**
         * @brief Client state machine states
         */
        enum class State {
            Idle,
        };

    public:
        ClientConnection(Server *parent, const int socketFd);
        ~ClientConnection();

        /**
         * @brief Check if the connection is dead
         *
         * Connections are marked dead when the client disconnects, or another error forces the
         * closure of the underlying socket.
         */
        constexpr inline bool isDead() {
            return this->deadFlag;
        }

        /**
         * @brief Get the RPC server this client belongs to
         */
        constexpr inline auto getServer() {
            return this->server;
        }

        void reply(std::span<const std::byte> payload);
        void reply(struct cbor_item_t* &root);

    private:
        void abort();

        void handleRead();
        void handleEvents(const TristLib::Event::Socket::Event);

        void sendRaw(std::span<const std::byte> payload);

    private:
        /// RPC server that we belong to
        Server *server{nullptr};

        /// Whether the connection is dead, and can be garbage collected
        bool deadFlag{false};

        /// Socket connection event
        std::shared_ptr<TristLib::Event::Socket> socket;

        /// Packet read buffer
        std::array<std::byte, kMaxPacketSize> rxBuffer;
};
}

#endif
