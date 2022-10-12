#ifndef GUI_DISPLAYMANAGER_H
#define GUI_DISPLAYMANAGER_H

#include <memory>

namespace Drivers::Display {
class Base;
}

namespace Gui {
/**
 * @brief GUI display manager
 *
 * This dude handles rendering stuff to the display.
 */
class DisplayManager {
    private:
        /// Display redraw interval (µsec)
        constexpr static const size_t kRedrawInterval{100 * 1000};

    public:
        DisplayManager(const std::shared_ptr<Drivers::Display::Base> &display);
        ~DisplayManager();

        /**
         * @brief Force rendering of a frame
         *
         * Redraw an entire frame.
         */
        void forceDraw() {
            this->draw(true);
        }

        /**
         * @brief Set the dirty flag
         */
        void setNeedsDisplay() {
            this->dirty = true;
        }

        void draw(const bool force = false);

    private:
        /// Set whenever the display needs a redraw
        bool dirty{false};

        /// Display to render to
        std::shared_ptr<Drivers::Display::Base> disp;

        /// Cairo surface to render to
        struct _cairo_surface *surface{nullptr};
        /// Cairo drawing context
        struct _cairo *ctx{nullptr};

        /// Display redraw timer
        struct event *redrawTimer{nullptr};
};
}

#endif
