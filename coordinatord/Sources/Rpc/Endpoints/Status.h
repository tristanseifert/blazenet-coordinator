#ifndef RPC_ENDPOINTS_STATUS_H
#define RPC_ENDPOINTS_STATUS_H

namespace Rpc {
class ClientConnection;
}

namespace Rpc::Endpoints {
/**
 * @brief Status endpoint
 *
 * Provides an interface to read the status of various components of the coordinator deamon and
 * its managed hardware.
 */
class Status {
    public:
        static void Handle(ClientConnection *client, const struct cbor_item_t *payload);

    private:
        static void GetRadioCounters(ClientConnection *, const struct cbor_item_t *);
};
}

#endif
