#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <fmt/format.h>
#include <TristLib/Core.h>
#include <TristLib/Event.h>

#include <cerrno>
#include <stdexcept>
#include <system_error>

#include "Config/Reader.h"
#include "Support/Confd.h"

#include "ClientConnection.h"
#include "Server.h"

using namespace Rpc;

/**
 * @brief Initialize the RPC server
 *
 * Open the local listening socket, and attach an event source that's used to accept new clients
 * to the run loop.
 */
Server::Server(const std::shared_ptr<Radio> &radio,
        const std::shared_ptr<Protocol::Handler> &protocol) : radio(radio), protocol(protocol) {
    // read some stuff from config
    auto socketPath = Config::GetConfig().at_path("rpc.listen");
    if(!socketPath || !socketPath.is_string()) {
        throw std::runtime_error("invalid configuration `rpc.listen`: expected string");
    }

    this->reloadConfig();

    // set up the socket
    this->initSocket(socketPath.value_or(""));

    // client management stuff
    this->initClientGc();
}

/**
 * @brief Initialize the listening socket
 *
 * Allocate a new SEQPACKET socket, and bind it to the path specified. It will be made non-blocking
 * for use with libevent.
 *
 * @param path Path for the socket file
 *
 * @remark Any existing file at the specified socket path will be deleted.
 */
void Server::initSocket(const std::string_view &path) {
    this->listenEvent = std::make_shared<TristLib::Event::ListenSocket>(
        TristLib::Event::RunLoop::Current(), [this](auto ev) {
            this->acceptClient();
    }, path, true, SOCK_SEQPACKET);
}

/**
 * @brief Initialize the client connection garbage collector
 *
 * This is a periodic timer that when it fires will deallocate all clients that are dead. This
 * will reclaim various resources associated with them.
 */
void Server::initClientGc() {
    this->clientGcTimer = std::make_shared<TristLib::Event::Timer>(
        TristLib::Event::RunLoop::Current(), kClientGcInterval, [this](auto timer) {
        this->numOffCycleGc = 0;
        this->garbageCollectClients();
    }, true);
}

/**
 * @brief Close down the RPC server
 *
 * Shut down the listening socket and terminate any still connected clients.
 */
Server::~Server() {
    // clean up client garbage collector
    this->clientGcTimer.reset();

    // TODO: shut down clients
    this->clients.clear();

    // shut down accept event
    this->listenEvent.reset();
}



/**
 * @brief Reload configuration
 *
 * Reads any dynamic configuration options, as well as additional stuff from the config file.
 */
void Server::reloadConfig() {
    // TODO: stuff
}



/**
 * @brief Accept a new client
 *
 * This first checks to see if we're not above the maximum threshold, and if so, accepts the client
 * and creates a client handler for it.
 */
void Server::acceptClient() {
    // accept a client socket
    int fd = this->listenEvent->accept();

    // if at capacity, close the socket again
    if(this->clients.size() > kMaxClients) {
        // garbage collect right now, if allowed
        if(++this->numOffCycleGc < kClientGcMaxOffcycle) {
            this->garbageCollectClients();
            if(this->clients.size() < kMaxClients) {
                goto beach;
            }
        }

        close(fd);

        ++this->numClientsRejected;
        throw std::runtime_error("maximum number of clients accepted!");
    }

beach:;
    // otherwise, create a client structure
    auto client = std::make_shared<ClientConnection>(this, fd);
    PLOG_DEBUG << fmt::format("accepted client: {}", static_cast<void *>(client.get()));

    // store it
    this->clients.emplace_back(std::move(client));
}

/**
 * @brief Perform client garbage collection
 *
 * Deallocate resources associated with all deceased clients.
 */
void Server::garbageCollectClients() {
    size_t count{0};

    std::erase_if(this->clients, [&count](const auto &client) -> bool {
        if(client->isDead()) {
            count++;
            return true;
        }
        return false;
    });

    PLOG_DEBUG_IF(count) << fmt::format("garbage collected {} client(s); {} total", count,
            this->clients.size());
}
