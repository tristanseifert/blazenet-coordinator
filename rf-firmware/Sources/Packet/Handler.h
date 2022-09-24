#ifndef PACKET_HANDLER_H
#define PACKET_HANDLER_H

#include <stddef.h>
#include <stdint.h>

#include <etl/array.h>
#include <etl/queue.h>
#include <etl/span.h>

namespace Packet {
/**
 * @brief Packet receive and transmit handler
 *
 * Implements scheduling of packets for transmission (based on different virtual queues for
 * priority) and buffering of received packets for read-out by the host.
 *
 * It doesn't know anything about the actual contents of the packets: this is handled in another
 * upper protocol layer.
 *
 * @note Calls into the handler are not thread safe, unless specified otherwise.
 */
class Handler {
    private:
        /// Log information about rejected receive packets
        constexpr static const bool kLogRxRejects{true};
        /// Log when a receive packet is queued
        constexpr static const bool kLogRx{false};
        /// Log information about rejected transmit packets
        constexpr static const bool kLogTxRejects{true};
        /// Log information about transmit queue packets
        constexpr static const bool kLogTx{true};

        /**
         * @brief Maximum packet data size
         *
         * This is fixed in the BlazeNet protocol: a packet may be up to 255 bytes in length.
         */
        constexpr static const size_t kMaxPacketSize{255};

        /**
         * @brief Maximum size to reserve for receive buffers
         *
         * Defines the maximum number of bytes to be allocated for use as a receive packet buffer.
         * This is approximate, as this does not include the overhead of heap allocations'
         * metadata.
         */
        constexpr static const size_t kMaxRxBufferSize{8*1024};

        /**
         * @brief Maximum number of receive queue slots to reserve
         *
         * This is the maximum number of packets that may be queued for reading by the host at a
         * given time.
         */
        constexpr static const size_t kMaxRxQueueSize{64};

        /**
         * @brief Maximum size to reserve for transmit buffers
         *
         * Defines the maximum number of bytes to be allocated for use as a transmit packet
         * buffer. This should be relatively small as the host can easily buffer transmit
         * packets.
         */
        constexpr static const size_t kMaxTxBufferSize{4*1024};

        /**
         * @brief Maximum number of transmit queue slots to reserve
         *
         * This is the maximum number of packets that may be pending to be transmitted for any
         * given priority level.
         */
        constexpr static const size_t kMaxTxQueueSize{16};

    public:
        /**
         * @brief Packet priority values
         *
         * Defines the priority of a transmit packet, in terms of which transmit queue it's
         * loaded into. Packets in higher priority queues will be transmitted before packets in
         * lower priority queues.
         */
        enum class TxPacketPriority: uint8_t {
            Background                          = 0x00,
            Normal                              = 0x01,
            RealTime                            = 0x02,
            NetworkControl                      = 0x03,
        };

        /**
         * @brief Transmit packet buffer structure
         *
         * Stores data for a packet to be transmitted over the air.
         */
        struct TxPacketBuffer {
            /**
             * @brief Size of the payload, in bytes
             */
            uint16_t packetSize{};

            /**
             * @brief Don't deallocate packet when discarding
             *
             * When set, the packet will _not_ be deallocated when it's being discarded after being
             * transmitted. This is useful for stuff like beacon frames and other periodic packets.
             */
            uint8_t isSticky                    :1{0};
            /// Flags that aren't assigned yet
            uint8_t reserved                    :7{0};

            /**
             * @brief Number of times CSMA failed for this packet
             *
             * This counter contains the number of times the packet could not be sent due to CSMA
             * failures.
             */
            uint8_t csmaFailCount{0};

            /**
             * @brief Packet payload
             *
             * This is the full contents of the packet, including MAC and PHY headers.
             */
            uint8_t data[];
        };

        /**
         * @brief Receive packet buffer structure
         *
         * Types of this structure are allocated to hold received packets. They contain a small
         * bit of metadata, as well as the actual packet payload.
         */
        struct RxPacketBuffer {
            /**
             * @brief Packet length
             *
             * Size of the packet, in bytes
             */
            uint16_t packetSize;

            /**
             * @brief Received signal strength
             *
             * RSSI of the packet, in full integer dBm
             */
            int8_t rssi;

            /**
             * @brief Link quality indication
             *
             * A relative value indicating the quality of the link on which the packet was
             * received, where 0 is really shitty and 255 is the absolute best.
             */
            uint8_t lqi;

            /**
             * @brief Payload
             *
             * This array contains the actual payload of the received packet, excluding any
             * preambles or PHY headers.
             */
            uint8_t data[];
        };

    private:
        using RxQueueType = etl::queue<RxPacketBuffer *, kMaxRxQueueSize,
              etl::memory_model::MEMORY_MODEL_SMALL>;
        using TxQueueType = etl::queue<TxPacketBuffer *, kMaxTxQueueSize,
              etl::memory_model::MEMORY_MODEL_SMALL>;

    public:
        static void Init();

        static int EneuqueTxPacket(const TxPacketPriority priority, TxPacketBuffer *packet);
        static TxPacketBuffer *EneuqueTxPacket(const TxPacketPriority priority,
                etl::span<const uint8_t> payload, const bool isSticky = false);
        static void DiscardTxPacket(TxPacketBuffer *, const bool force = false);

        static RxPacketBuffer *EnqueueRxPacket(const struct RAIL_RxPacketInfo &,
                const struct RAIL_RxPacketDetails &);
        static void DiscardRxPacket(RxPacketBuffer *);

        /**
         * @brief Peek at the first packet in the receive queue
         *
         * Returns the first (oldest) packet in the receive queue, without popping it.
         */
        static inline RxPacketBuffer *PeekRxQueue() {
            return gRxQueue->front();
        }

        /**
         * @brief Pop the first packet from the receive queue
         *
         * Returns the first (oldest) packet in the receive queue, and then removes it from the
         * queue.
         *
         * @remark Be sure to call DiscardRxPacket when done to release the packet's memory.
         *
         * @seeAlso DiscardRxPacket
         */
        static inline auto PopRxQueue() {
            RxPacketBuffer *buf = gRxQueue->front();
            gRxQueue->pop();
            UpdateRxQueueState();

            return buf;
        }

        /**
         * @brief Get the receive queue overflow flag
         */
        static inline bool GetRxOverflowFlag() {
            return gRxOverflowFlag;
        }
        /**
         * @brief Is the receive queue empty?
         */
        static inline bool GetRxEmptyFlag() {
            return gRxQueue->empty();
        }
        /**
         * @brief Is the receive queue full?
         */
        static inline bool GetRxFullFlag() {
            return gRxQueue->full();
        }

        /**
         * @brief Pop the next packet from the transmit queue
         *
         * This searches the queues in descending priority order, e.g. the highest priority queue
         * will be serviced before lower priority queues.
         *
         * @return Packet from queue, or `nullptr` if no packets pending
         *
         * @remark Be sure to call DiscardTxPacket when done to release the packet's memory.
         *
         * @seeAlso DiscardTxPacket
         */
        static inline TxPacketBuffer *PopTxQueue() {
            for(size_t i = 0; i < 4; i++) {
                auto queue = gTxQueues[3 - i];
                if(queue->empty()) {
                    continue;
                }

                TxPacketBuffer *buf = queue->front();
                queue->pop();
                return buf;
            }

            // no packets pending
            return nullptr;
        }

        /**
         * @brief Get the transmit queue overflow flag
         */
        static inline bool GetTxOverflowFlag() {
            return gTxOverflowFlag;
        }
        /**
         * @brief Is the receive queue empty?
         */
        static inline bool GetTxEmptyFlag() {
            return !gTxPacketsPending;
        }

    private:
        static void UpdateRxQueueState();
        static void UpdateTxQueueState();

        static int SubmitTxPacket(TxQueueType *, TxPacketBuffer *);

    private:
        /// Rx queue overflow flag (sticky)
        static bool gRxOverflowFlag;
        /// Total number of bytes allocated for receive packet buffers
        static size_t gRxAllocBytes;
        /// Number of rx packets discarded because the buffer alloc limit has been reached
        static size_t gRxBufferDiscarded;
        /// Number of rx packets discarded because of allocation failures
        static size_t gRxBufferAllocFailed;
        /// Number of rx packets discarded because rx queue is full
        static size_t gRxQueueDiscarded;
        /// Queue holding pointers to all received packets
        static RxQueueType *gRxQueue;

        /// Tx queue overflow flag (sticky)
        static bool gTxOverflowFlag;
        /// Total number of bytes allocated for transmit packet buffers
        static size_t gTxAllocBytes;
        /// Number of tx packets discarded because the buffer alloc limit has been reached
        static size_t gTxBufferDiscarded;
        /// Number of tx packets discarded because of allocation failures
        static size_t gTxBufferAllocFailed;
        /// Number of tx packets discarded because rx queue is full
        static size_t gTxQueueDiscarded;
        /// Total number of packets pending (if 0, transmit directly)
        static size_t gTxPacketsPending;
        /// Containers for receive queues (in ascending priority order)
        static etl::array<TxQueueType *, 4> gTxQueues;
};
}

#endif
