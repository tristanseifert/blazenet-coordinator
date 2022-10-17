#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#include <cbor.h>
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <fmt/format.h>

#include <span>
#include <stdexcept>

#include "Support/EventLoop.h"
#include "Support/Logging.h"

#include "Endpoints/Config.h"
#include "Server.h"
#include "Types.h"
#include "ClientConnection.h"

using namespace Rpc;

/**
 * @brief Initialize a new client connection
 *
 * We'll create a libevent "buffer event" to handle asynchronously pushing data into the
 * connection, and being notified when events take place on the connection. These events will
 * call back into the client connection instance.
 */
ClientConnection::ClientConnection(Server *parent, const int socketFd) : server(parent),
    socket(socketFd) {
    auto evbase = Support::EventLoop::Current()->getEvBase();

    // create buffer event
    auto bev = bufferevent_socket_new(evbase, socketFd, BEV_OPT_CLOSE_ON_FREE);
    if(!bev) {
        throw std::runtime_error("failed to create bufevent");
    }

    this->event = bev;

    // configure it such that we don't get woken up til an RPC header is read
    bufferevent_setwatermark(this->event, EV_READ, sizeof(Rpc::RequestHeader),
            EV_RATE_LIMIT_MAX);

    // install callbacks
    bufferevent_setcb(this->event, [](auto bev, auto ctx) {
        auto cli = reinterpret_cast<ClientConnection *>(ctx);
        try {
            cli->handleRead();
        } catch(const std::exception &e) {
            PLOG_ERROR << fmt::format("client {} read failed: {}", ctx, e.what());
            cli->abort();
        }
    }, nullptr, [](auto bev, auto what, auto ctx) {
        auto cli = reinterpret_cast<ClientConnection *>(ctx);
        try {
            cli->handleEvent(what);
        } catch(const std::exception &e) {
            PLOG_ERROR << fmt::format("client {} event failed: {}", ctx, e.what());
            cli->abort();
        }
    }, this);

    // enable event for "client data available to read" events
    int err = bufferevent_enable(this->event, EV_READ);
    if(err == -1) {
        throw std::runtime_error("failed to enable bufferevent");
    }
}

/**
 * @brief Clean up client resources
 *
 * Close the socket (if not already done) and release the libevent resources.
 */
ClientConnection::~ClientConnection() {
    if(this->event) {
        bufferevent_free(this->event);
    }
}



/**
 * @brief Sets the connection to an error state
 *
 * This will ensure the client connection is closed on the next garbage collection pass, and we'll
 * go ahead and close the socket already.
 */
void ClientConnection::abort() {
    // set error flags
    this->deadFlag = true;

    // delete the buffer event: this will close the socket as well
    bufferevent_free(this->event);
    this->event = nullptr;
}



/**
 * @brief Read packet from connection
 *
 * Attempt to read a full packet from the connection. Due to the watermark config, it's guaranteed
 * that when invoked, we have at least an RPC connection header waiting to be read out.
 *
 * If the packet is valid, we'll attempt to CBOR decode the payload (if any.)
 */
void ClientConnection::handleRead() {
    // prepare the receive buffer
    auto buf = bufferevent_get_input(this->event);
    const size_t pending = evbuffer_get_length(buf);

    this->rxBuffer.resize(pending);

    auto read = evbuffer_remove(buf, static_cast<void *>(this->rxBuffer.data()), pending);
    if(read == -1) {
        throw std::runtime_error("evbuffer_remove failed");
    }

    // validate the header
    if(read < sizeof(RequestHeader)) {
        // should never happen…
        throw std::runtime_error(fmt::format("insufficient RPC read: {}", read));
    }

    auto hdr = reinterpret_cast<const RequestHeader *>(this->rxBuffer.data());

    if(hdr->version != kCurrentVersion) {
        throw std::runtime_error(fmt::format("invalid rpc version: ${:04x}", hdr->version));
    } else if(hdr->length < sizeof(*hdr) || hdr->length > this->rxBuffer.size()) {
        throw std::runtime_error(fmt::format("invalid header size ({}, have {})", hdr->length,
                    this->rxBuffer.size()));
    }

    // get payload
    std::span<const std::byte> payload(reinterpret_cast<const std::byte *>(hdr->payload),
            hdr->length - sizeof(*hdr));

    // decode it as CBOR
    cbor_item_t *cborItem{nullptr};

    if(!payload.empty()) {
        struct cbor_load_result result{};
        cborItem = cbor_load(reinterpret_cast<const cbor_data>(payload.data()), payload.size(),
                &result);

        if(result.error.code != CBOR_ERR_NONE) {
            throw std::runtime_error(fmt::format("cbor_load failed: {} (at ${:x})",
                        result.error.code, result.error.position));
        }
    }

    // invoke handler
    try {
        switch(hdr->endpoint) {
            case RequestEndpoint::Config:
                Endpoints::Config::Handle(this, cborItem);
                break;

            // unimplemented endpoint
            default:
                throw std::runtime_error(fmt::format("unknown rpc endpoint ${:02x}",
                            hdr->endpoint));
        }
    } catch(const std::exception &) {
        // ensure we don't leak the CBOR item, if allocated
        if(cborItem) {
            cbor_decref(&cborItem);
        }
        throw;
    }

    if(cborItem) {
        cbor_decref(&cborItem);
    }
}

/**
 * @brief Handle a client connection event
 *
 * This likely corresponds to the connection being closed, or an IO error.
 *
 * @param flags A logical OR of `BEV_EVENT_` flags
 */
void ClientConnection::handleEvent(const size_t flags) {
    // events that will close the connection
    if((flags & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) != 0) {
        PLOG_DEBUG << fmt::format("client {}: close due to {}", static_cast<void *>(this),
                (flags & BEV_EVENT_EOF) ? "EoF" : "IO error");
        return this->abort();
    }
    // TODO: do we need to handle BEV_EVENT_READING or BEV_EVENT_WRITING?
}



/**
 * @brief Slap an RPC header in front of the payload and send it
 *
 * @param payload Data to transmit
 */
void ClientConnection::reply(std::span<const std::byte> payload) {
    std::vector<std::byte> buffer;
    buffer.resize(sizeof(struct RequestHeader) + payload.size(), std::byte{0});

    // get last header
    if(this->rxBuffer.size() < sizeof(RequestHeader)) {
        throw std::runtime_error("invalid rx packet buffer (for tag/ep)");
    }
    auto lastHdr = reinterpret_cast<const RequestHeader *>(this->rxBuffer.data());

    // build up the header
    auto hdr = reinterpret_cast<struct RequestHeader *>(buffer.data());
    hdr->version = kCurrentVersion;
    hdr->length = sizeof(*hdr) + payload.size();
    hdr->endpoint = lastHdr->endpoint;
    hdr->tag = lastHdr->tag;

    // copy payload
    if(!payload.empty()) {
        std::copy(payload.begin(), payload.end(), buffer.begin() + sizeof(*hdr));
    }

    // send and return tag
    this->sendRaw(buffer);
}

/**
 * @brief Serialize CBOR item and send as reply
 *
 * @param root CBOR item to serialize
 *
 * @remark This method will decrement a reference on the CBOR item, meaning that if the caller
 *         does not increment the refcount beforehand, will lead to it being deallocated.
 */
void ClientConnection::reply(cbor_item_t* &root) {
    size_t rootBufLen;
    unsigned char *rootBuf{nullptr};
    const size_t serializedBytes = cbor_serialize_alloc(root, &rootBuf, &rootBufLen);
    cbor_decref(&root);

    try {
        this->reply({reinterpret_cast<std::byte *>(rootBuf), serializedBytes});
        free(rootBuf);
    } catch(const std::exception &) {
        free(rootBuf);
        throw;
    }
}

/**
 * @brief Send a raw packet to the remote
 *
 * This assumes the packet already has a `struct RpcHeader` prepended.
 */
void ClientConnection::sendRaw(std::span<const std::byte> payload) {
    // TODO: should we write to the buffer event instead?
    int err = write(this->socket, payload.data(), payload.size());
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write");
    }
}
