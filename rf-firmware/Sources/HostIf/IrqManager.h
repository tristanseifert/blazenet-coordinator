#ifndef HOSTIF_IRQMANAGER_H
#define HOSTIF_IRQMANAGER_H

#include <stddef.h>
#include <stdint.h>

#include "bitflags.h"

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
     *
     * Clear: Read status register
     */
    CommandError                                = (1 << 0),

    /**
     * @brief Packet received
     *
     * Set: A packet has been received
     *
     * Clear: Read out all pending packets
     */
    PacketReceived                              = (1 << 1),
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
         * @brief Assert (set) an interrupt line
         *
         * Marks the specified interrupt lines as being asserted, and updates the physical
         * interrupt line.
         *
         * @param which Interrupt line(s) to be asserted
         */
        static inline void Assert(const Interrupt which) {
            gActive |= which;
            Update();
        }

        /**
         * @brief Deassert (clear) an interrupt line
         *
         * Marks the specified interrupt lines as being deasserted, and updates the physical
         * interrupt line.
         *
         * @param which Interrupt line(s) to be deasserted
         */
        static inline void Deassert(const Interrupt which) {
            gActive &= ~which;
            Update();
        }

    private:
        static void Update();

    private:
        /// Active interrupt lines
        static Interrupt gActive;
        /// Current interrupt mask
        static Interrupt gMask;
};
}

#endif
