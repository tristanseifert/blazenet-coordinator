#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <chrono>
#include <list>
#include <memory>

namespace TristLib::Event {
class Timer;
}

class Radio;
namespace Protocol {
class Handler;
}

namespace Rpc {
class ClientConnection;

/**
 * @brief Local RPC server
 *
 * Implements the local RPC server, which listens on an UNIX domain socket for requests from other
 * locally running software. It's mainly intended for use by the local UI and web interface, as
 * well as some command line tools and the remote interface adapters.
 */
class Server {
    public:
        Server(const std::shared_ptr<Radio> &radio,
                const std::shared_ptr<Protocol::Handler> &protocol);
        ~Server();

        void reloadConfig();

        /**
         * @brief Get the radio instance
         */
        inline auto getRadio() {
            return this->radio.lock();
        }
        /**
         * @brief Get the BlazeNet protocol handler
         */
        inline auto getProtocol() {
            return this->protocol.lock();
        }

    private:
        void initSocket(const std::string_view &listenPath);
        void listen();
        void initListenEvent();

        void initClientGc();
        void garbageCollectClients();

        void acceptClient();

    private:
        /// Maximum amount of clients that may be waiting to be accepted at once
        constexpr static const size_t kListenBacklog{5};
        /// Maximum number of simultaneous connected clients
        constexpr static const size_t kMaxClients{100};
        /// Interval for client garbage collection (sec)
        constexpr static const std::chrono::seconds kClientGcInterval{15};
        /// Maximum number of times garbage collection can be invoked between scheduled intervals
        constexpr static const size_t kClientGcMaxOffcycle{10};

        /// Radio abstraction layer
        std::weak_ptr<Radio> radio;
        /// BlazeNet protocol handler
        std::weak_ptr<Protocol::Handler> protocol;

        /// RPC listening socket
        int listenSock{-1};
        /// Event for the listening socket (triggered on accept)
        struct event *listenEvent{nullptr};

        /// Timer event to garbage collect disconnected clients
        std::shared_ptr<TristLib::Event::Timer> clientGcTimer;
        /// List containing all connected clients
        std::list<std::shared_ptr<ClientConnection>> clients;

        /// Number of times garbage collection has ran since the last timer event
        size_t numOffCycleGc{0};
        /// Number of clients rejected because we're at capacity
        size_t numClientsRejected{0};
};
}

#endif
