#ifndef RPC_BLAZEDCLIENT_H
#define RPC_BLAZEDCLIENT_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Rpc {
/**
 * @brief blazed local RPC client
 *
 * Interfaces to the local blazed rpc endpoint. The exposed interface will block the caller for the
 * duration of the request.
 */
class BlazedClient {
    private:
        /// Maximum size of an RPC packet
        constexpr static const size_t kMaxPacketSize{4096};

        /**
         * @brief Current supported RPC version
         */
        constexpr static const uint16_t kCurrentVersion{0x0100};

        /**
         * @brief Header structure prepended to all RPC requests
         */
        struct RequestHeader {
            /**
             * @brief RPC version
             *
             * @seeAlso kCurrentVersion
             */
            uint16_t version;

            /**
             * @brief Total request size, in bytes
             */
            uint16_t length;

            /**
             * @brief Message endpoint
             */
            uint8_t endpoint;

            /**
             * @brief Message tag
             */
            uint8_t tag;

            /**
             * @brief Payload data
             */
            uint8_t payload[];
        } __attribute__((packed));

        /**
         * @brief RPC endpoints
         */
        enum RequestEndpoint: uint8_t {
            /// Read running configuration
            Config                      = 0x01,
            /// Get status of various components
            Status                      = 0x02,
        };

    public:
        static std::shared_ptr<BlazedClient> &The();
        static void CleanUp();

        // don't call this; use the shared The() method!
        BlazedClient();
        ~BlazedClient();

        void getVersion(std::string &outVersion, std::string &outBuild,
                std::string &outRadioVersion);

        void getRadioConfig(std::string &outRegion, size_t &outChannel, double &outTxPower);

        void getRadioStats(size_t &outRxGood, size_t &outRxCorrupt, size_t &outRxFifoOverruns,
                size_t &outTxGood, size_t &outTxCcaFails, size_t &outTxFifoUnderruns);
        void getClientStats(size_t &outNumConnected);

    private:
        void ensureConnection();
        void connect();
        void tearDown();
        void sendRaw(std::span<const std::byte>);

        uint8_t sendPacket(const uint8_t, struct cbor_item_t* &);
        uint8_t sendPacket(const uint8_t, std::span<const std::byte>);
        [[nodiscard]] struct cbor_item_t *readResponse(const uint8_t tag);

        /**
         * @brief Send a packet and read response
         *
         * @param endpoint Endpoint to send the packet to
         * @param root CBOR item to serialize as the root
         *
         * @return Deserialized CBOR payload, if any
         *
         * @remark Caller is responsible for deallocating the returned CBOR item
         */
        inline auto sendWithResponse(const uint8_t endpoint, struct cbor_item_t* &root) {
            const auto tag = this->sendPacket(endpoint, root);
            return this->readResponse(tag);
        }

    private:
        /// Path for the RPC socket file
        std::filesystem::path socketPath;
        /// File descriptor for the local RPC socket
        int socket{-1};

        /// Tag for the next outgoing message
        uint8_t nextTag{0};

        /// Packet receive buffer
        std::vector<std::byte> rxBuffer;
};
}

#endif
