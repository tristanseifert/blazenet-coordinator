#ifndef RPC_CLIENTCONNECTION_H
#define RPC_CLIENTCONNECTION_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Rpc {
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
        /**
         * @brief Client state machine states
         */
        enum class State {
            Idle,
        };

    public:
        ClientConnection(const int socketFd);
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

    private:
        void abort();

        void handleRead();
        void handleEvent(const size_t eventFlags);

    private:
        /// Whether the connection is dead, and can be garbage collected
        bool deadFlag{false};

        /// Socket used to communicate with the client
        int socket{-1};
        /// Buffer event wrapping the socket
        struct bufferevent *event{nullptr};

        /// Packet read buffer
        std::vector<std::byte> rxBuffer;
};
}

#endif
