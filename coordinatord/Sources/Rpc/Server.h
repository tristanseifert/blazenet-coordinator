#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <memory>

class Radio;
namespace Protocol {
class Handler;
}

namespace Rpc {
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

    private:
        void initSocket(const std::string_view &listenPath);
        void listen();
        void initListenEvent();

        void acceptClient();

    private:
        /// Maximum amount of clients that may be waiting to be accepted at once
        constexpr static const size_t kListenBacklog{5};
        /// Maximum number of simultaneous connected clients
        constexpr static const size_t kMaxClients{100};

        /// Radio abstraction layer
        std::weak_ptr<Radio> radio;
        /// BlazeNet protocol handler
        std::weak_ptr<Protocol::Handler> protocol;

        /// RPC listening socket
        int listenSock{-1};
        /// Event for the listening socket (triggered on accept)
        struct event *listenEvent{nullptr};
};
}

#endif
