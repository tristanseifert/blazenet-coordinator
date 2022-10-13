#ifndef GUI_SCREEN_H
#define GUI_SCREEN_H

namespace Gui {
class DisplayManager;

/**
 * @brief A screen on the external display to render
 *
 * Screens encapsulate the logic for drawing what's to be displayed on the external display of
 * the device. Each screen is self contained, and will consume all input.
 */
class Screen {
    public:
        virtual ~Screen() = default;

        /**
         * @brief Render the screen
         *
         * Draw screen contents into the specified drawing context.
         *
         * @param ctx Drawing context to use
         * @param dirty Whether the drawing is for a reason other than the screen being dirty.
         */
        virtual void draw(struct _cairo *ctx, const bool dirty) = 0;

        /**
         * @brief Whether the screen needs redrawing
         */
        virtual bool isDirty() = 0;

        /// @{ @name Callbacks
        /**
         * @brief Screen is about to disappear
         */
        virtual void willDisappear(DisplayManager *) {

        }

        /**
         * @brief DisplayManager is about to appear
         */
        virtual void willAppear(DisplayManager *mgr) {
            this->manager = mgr;
        }

        /**
         * @brief DisplayManager has disappeared
         */
        virtual void didDisappear(DisplayManager *) {
            this->manager = nullptr;
        }

        /**
         * @brief DisplayManager has appeared
         */
        virtual void didAppear(DisplayManager *) {

        }
        /// @}
        //
    protected:
        /**
         * @brief Display manager presenting this screen
         *
         * This variable is set to the currently presenting display manager. Use this to force a
         * redraw when external data causes a change.
         */
        DisplayManager *manager{nullptr};
};
}

#endif
