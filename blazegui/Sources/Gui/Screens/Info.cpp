#include <cairo.h>
#include <event2/event.h>
#include <fmt/format.h>

#include <stdexcept>

#include "Gui/DisplayManager.h"
#include "Gui/TextRenderer.h"
#include "Support/EventLoop.h"
#include "Support/Logging.h"

#include "Info.h"

using namespace Gui::Screens;

/**
 * @brief Release drawing resources
 */
Info::~Info() {
    if(this->bgPattern) {
        cairo_pattern_destroy(this->bgPattern);
    }

    if(this->timer) {
        event_del(this->timer);
        event_free(this->timer);
    }
}

/**
 * @brief View is about to appear
 *
 * Set up and start the page flip timer
 */
void Info::didAppear(DisplayManager *mgr) {
    Screen::didAppear(mgr);

    // kill old one, if any
    if(this->timer) {
        event_del(this->timer);
        event_free(this->timer);
    }

    // create it
    auto evbase = Support::EventLoop::Current()->getEvBase();
    this->timer = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<Info *>(ctx)->flipPage();
    }, this);
    if(!this->timer) {
        throw std::runtime_error("failed to allocate timer");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(kPageFlipInterval / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(kPageFlipInterval % 1'000'000U),
    };

    evtimer_add(this->timer, &tv);
}

/**
 * @brief View will disappear
 *
 * This kills the page flipping timer
 */
void Info::willDisappear(DisplayManager *mgr) {
    Screen::willDisappear(mgr);

    // kill the timer
    if(this->timer) {
        event_del(this->timer);
        event_free(this->timer);

        this->timer = nullptr;
    }
}

/**
 * @brief Draw the info display
 *
 * This starts by drawing a common background, then invokes the appropriate drawing function for
 * the stuff to show on top of that.
 */
void Info::draw(cairo_t *ctx, const bool dirty) {
    // create resources, if needed
    if(!this->hasResources) {
        this->initResources(ctx);
        this->hasResources = true;
    }

    TextRenderer text(ctx);

    // fill bg
    cairo_set_source(ctx, this->bgPattern);
    //cairo_rectangle(ctx, 0, 0, 240, 240);
    //cairo_fill(ctx);
    cairo_paint(ctx);

/*
    cairo_set_source_rgb(ctx, 0, 1, 1);
    cairo_move_to(ctx, 0, 0);
    cairo_line_to(ctx, 120, 120);
    cairo_set_line_width(ctx, 5);
    cairo_stroke(ctx);
*/

    // draw on top
    switch(this->page) {
        case Section::Network:
            this->drawPageNetwork(ctx, text);
            break;
    }

    // clear dirty flag
    this->dirtyFlag = false;
}

/**
 * @brief Initialize Cairo resources
 *
 * This sets up the background gradient and font rendering stuff.
 */
void Info::initResources(cairo_t *ctx) {
    // bg pattern
    auto pat = cairo_pattern_create_linear(120, 0, 120, 240.);
    if(cairo_pattern_status(pat) != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error(fmt::format("cairo_pattern_create_linear failed: {}",
                    cairo_pattern_status(pat)));
    }

    cairo_pattern_add_color_stop_rgb(pat, 0.,   0., 0., 0.);
    cairo_pattern_add_color_stop_rgb(pat, 0.6, 0., 0., 0.);
    cairo_pattern_add_color_stop_rgb(pat, 1.0,  0., 0., .74);

    this->bgPattern = pat;
}

/**
 * @brief Flip the displayed information page
 */
void Info::flipPage() {
    // mark as dirty
    this->dirtyFlag = true;
}



/**
 * @brief Draw the network page
 *
 * Show the IP addresses of active network interfaces.
 */
void Info::drawPageNetwork(cairo_t *ctx, TextRenderer &text) {
    // section header
    DrawTitle(ctx, text, "IP Status");

    // network interfaces

}



/**
 * @brief Draw a screen title
 */
void Info::DrawTitle(cairo_t *ctx, TextRenderer &text, const std::string_view &str) {
    text.setFont("DINish Condensed Bold", 24);
    text.draw(ctx, {0, 0}, {240, 28}, {1, 1, 1}, str,
            TextRenderer::HorizontalAlign::Center, TextRenderer::VerticalAlign::Top);
}
