#ifndef GUI_SCREENS_INFO_H
#define GUI_SCREENS_INFO_H

#include <cstddef>
#include <string_view>

#include "Gui/Screen.h"

namespace Gui {
class TextRenderer;
}

namespace Gui::Screens {
/**
 * @brief Primary information display
 *
 * Renders a series of informational screens about the system on the display. This is driven by a
 * timer that fires periodically.
 */
class Info: public Screen {
    private:
        /// Page update interval (µsec)
        constexpr static const size_t kPageFlipInterval{1 * 1000 * 1000};
        /// Numberof update cycles before switching
        constexpr static const size_t kPageFlipCycles{15};

        /// Information section to display
        enum class Section: size_t {
            /// IP Network configuration
            Network                     = 0,
            /// BlazeNet status
            BlazeNetStatus              = 1,
            /// BlazeNet traffic load
            BlazeNetTraffic             = 2,
            /// System status (load, uptime, memory)
            SystemStatus                = 3,
            /// Firmware versions
            Versions                    = 4,

            Last                        = Versions,
        };

    public:
        ~Info();

        void draw(struct _cairo *ctx, const bool dirty) override;

        /// Return the state of a flag that's updated when the timer changes the page
        bool isDirty() override {
            return this->dirtyFlag;
        }

        void didAppear(DisplayManager *) override;
        void willDisappear(DisplayManager *) override;

    private:
        void initResources(struct _cairo *);

        void timerFired();
        void flipPage();

        void drawPageNetwork(struct _cairo *, TextRenderer &);
        void drawPageBlazeStatus(struct _cairo *, TextRenderer &);
        void drawPageBlazeTraffic(struct _cairo *, TextRenderer &);
        void drawPageSysStatus(struct _cairo *, TextRenderer &);
        void drawPageVersions(struct _cairo *, TextRenderer &);

        static void DrawTitle(struct _cairo *, TextRenderer &, const std::string_view &);
        static void DrawFooter(struct _cairo *, TextRenderer &);

    private:
        /// Set when the page has been changed
        bool dirtyFlag{true};
        /// Set when all Cairo resources have been created
        bool hasResources{false};
        /// Is automatic page cycling enabled?
        bool cyclingEnabled{true};

        /// Current info page to display
        Section page{Section::Network};
        /// Periodic update timer (to cycle through screens)
        struct event *timer{nullptr};
        /// Number of redraw cycles we've gone through
        size_t pageCycles{0};

        /// Background gradient pattern
        struct _cairo_pattern *bgPattern{nullptr};
};
}

#endif
