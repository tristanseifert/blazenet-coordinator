#ifndef RPC_ENDPOINTS_CONFIG_H
#define RPC_ENDPOINTS_CONFIG_H

namespace Rpc {
class ClientConnection;
}

namespace Rpc::Endpoints {
/**
 * @brief Configuration endpoint
 *
 * Allows reading out the configuration of various elements
 */
class Config {
    public:
        static void Handle(ClientConnection *client, const struct cbor_item_t *payload);

    private:
        static void GetRadioCfg(ClientConnection *, const struct cbor_item_t *);
        static void GetVersion(ClientConnection *, const struct cbor_item_t *);

        static void Reply(ClientConnection *, struct cbor_item_t* &);
};
}

#endif
