#ifndef HOSTIF_IRQMANAGER_H
#define HOSTIF_IRQMANAGER_H

#include <stddef.h>
#include <stdint.h>

#include "bitflags.h"
#include "Rtos/Rtos.h"

namespace HostIf {
/**
 * @brief Interrupt bits
 *
 * This enum defines bits for all supported interrupts. These may be combined via bitwise-OR.
 */
enum class Interrupt: uintptr_t {
    /// No interrupts active
    None                                        = 0,

    /**
     * @brief Command error
     *
     * Set: A command failed with an error code.
     */
    CommandError                                = (1 << 0),

    /**
     * @brief Packet received
     *
     * Set: A packet has been received
     */
    PacketReceived                              = (1 << 1),

    /**
     * @brief Packet transmitted
     *
     * Set: A packet was transmitted
     */
    PacketTransmitted                           = (1 << 2),

    /**
     * @brief Transmit queue is empty
     *
     * Set: All pending packets are transmitted
     */
    TxQueueEmpty                                = (1 << 3),
};
ENUM_FLAGS_EX(Interrupt, uintptr_t);

/**
 * @brief This dude is responsible for the external interrupt line
 *
 * It has an internal collection of interrupt flags (managed by external parts of the codebase)
 * and combines them logically for the output interrupt line state, taking into account the
 * interrupt state mask.
 *
 * @remark When updating interrupt flags, be sure that all other registers or user visible state is
 *         updated before. Otherwise, the host's interrupt handler may read stale data.
 */
class IrqManager {
    private:
        /// Whether IRQ state changes are logged
        constexpr static const bool kLogChanges{false};

        /// How many ticks an irq may be pending for before it's considered lost
        constexpr static const size_t kIrqThreshold{pdMS_TO_TICKS(50)};

    public:
        static void Init();

        /**
         * @brief Set the interrupt mask
         *
         * Defines which interrupts will affect the physical interrupt line.
         *
         * @param newMask Interrupt mask to apply; all interrupts set in this mask will cause the
         *                physical interrupt line to update.
         */
        static inline void SetMask(const Interrupt newMask) {
            gMask = newMask;
            Update();
        }

        /**
         * @brief Get current interrupt mask
         */
        static inline auto GetMask() {
            return gMask;
        }

        /**
         * @brief Get pending interrupts
         *
         * Returns all pending interrupts that aren't masked.
         */
        static inline auto GetPending() {
            return gMaskedActive;
        }

        /**
         * @brief Assert (set) an interrupt line
         *
         * Marks the specified interrupt lines as being asserted, and updates the physical
         * interrupt line.
         *
         * @param which Interrupt line(s) to be asserted
         */
        static inline void Assert(const Interrupt which) {
            taskENTER_CRITICAL();
            gActive |= which;
            Update();
            taskEXIT_CRITICAL();
        }

        /**
         * @brief Deassert (clear) an interrupt line
         *
         * Marks the specified interrupt lines as being deasserted, and updates the physical
         * interrupt line.
         *
         * @param which Interrupt line(s) to be deasserted
         */
        static inline void Acknowledge(const Interrupt which) {
            taskENTER_CRITICAL();
            gActive &= ~which;

            // clear the lost irq recovery state machine
            gTicksPending = gPendingStage = 0;
            gLostIrqRecovery = false;

            Update();
            taskEXIT_CRITICAL();
        }

        static void TickCallback();

    private:
        static void Update();

    private:
        /// Active interrupt lines
        static Interrupt gActive;
        /// Current interrupt mask
        static Interrupt gMask;
        /// Masked, active interrupts
        static Interrupt gMaskedActive;

        /// Are we currently trying to recover from a lost irq?
        static bool gLostIrqRecovery;
        /// Number of ticks an irq has been pending
        static uint16_t gTicksPending;
        /// Current stage of irq handling
        static uint16_t gPendingStage;
};
}

#endif
