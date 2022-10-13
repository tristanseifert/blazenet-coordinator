#include <cairo.h>
#include <event2/event.h>

#include <fmt/format.h>

#include <stdexcept>

#include "Drivers/Display/Base.h"
#include "Support/EventLoop.h"
#include "Support/Logging.h"

#include "DisplayManager.h"
#include "Screen.h"

using namespace Gui;


DisplayManager::DisplayManager(const std::shared_ptr<Drivers::Display::Base> &display) :
    disp(display) {
    // set up a cairo surface
    cairo_format_t fmt;
    switch(display->getBitsPerPixel()) {
        case 16:
            fmt = CAIRO_FORMAT_RGB16_565;
            break;
        default:
            throw std::runtime_error(fmt::format("unsupported bpp: {}",
                        display->getBitsPerPixel()));
    }

    auto surf = cairo_image_surface_create_for_data(display->getFramebuffer().data(), fmt,
            display->getWidth(), display->getHeight(), display->getFramebufferStride());

    if(cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error(fmt::format("cairo_image_surface_create_for_data failed: {}",
                    cairo_surface_status(surf)));
    }

    this->surface = surf;

    // create a context from it
    auto cairo = cairo_create(surf);
    if(cairo_status(cairo) != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error(fmt::format("cairo_create failed: {}", cairo_status(cairo)));
    }

    this->ctx = cairo;

    // clear the background
    cairo_set_source_rgb(this->ctx, 0.2, 0.2, 0.2);
    cairo_paint(this->ctx);

    // set up a timer to periodically redraw the display
    auto evbase = Support::EventLoop::Current()->getEvBase();
    this->redrawTimer = event_new(evbase, -1, EV_PERSIST, [](auto, auto, auto ctx) {
        reinterpret_cast<DisplayManager *>(ctx)->draw();
    }, this);
    if(!this->redrawTimer) {
        throw std::runtime_error("failed to allocate redraw timer");
    }

    struct timeval tv{
        .tv_sec  = static_cast<time_t>(kRedrawInterval / 1'000'000U),
        .tv_usec = static_cast<suseconds_t>(kRedrawInterval % 1'000'000U),
    };

    evtimer_add(this->redrawTimer, &tv);
}

/**
 * @brief Clean up drawing context
 */
DisplayManager::~DisplayManager() {
    if(this->redrawTimer) {
        event_del(this->redrawTimer);
        event_free(this->redrawTimer);
    }

    cairo_destroy(this->ctx);
    cairo_surface_destroy(this->surface);
}


/**
 * @brief Render a frame
 *
 * This renders the current GUI scene into the drawing context, and transfers it to the display.
 */
void DisplayManager::draw(const bool force) {
    // bail if not dirty
    if(!force) {
        if(!this->dirty &&
                (!this->currentScreen || (this->currentScreen && !this->currentScreen->isDirty()))) {
            return;
        }
    }

    // prepare context
    cairo_save(this->ctx);

    // draw
    if(this->currentScreen) {
        this->currentScreen->draw(this->ctx, force || this->dirty);
    } else {
        // indicates that there is no screen to render
        cairo_set_source_rgb(this->ctx, 0.33, 0.33, 1);
        cairo_paint(this->ctx);
    }

    // restore context state
    cairo_restore(this->ctx);

    this->dirty = false;

    // flush context, byteswap, and push to display
    cairo_surface_flush(this->surface);

    auto buf = this->disp->getFramebuffer();
    const auto words = buf.size() / 2;
    for(size_t i = 0; i < words; i++) {
        uint16_t temp;
        memcpy(&temp, buf.data() + (i * 2), sizeof(temp));
        temp = __builtin_bswap16(temp);
        memcpy(buf.data() + (i * 2), &temp, sizeof(temp));
    }

    this->disp->transferBuffer();
}

/**
 * @brief Update the displayed screen
 *
 * Ensures the display is redrawn at the next opportunity.
 *
 * @param newScreen New screen to render
 */
void DisplayManager::setScreen(const std::shared_ptr<Screen> &newScreen) {
    auto old = this->currentScreen;
    if(old) {
        old->willDisappear(this);
    }
    newScreen->willAppear(this);

    this->currentScreen = newScreen;

    newScreen->didAppear(this);
    if(old) {
        old->didDisappear(this);
    }

    this->setNeedsDisplay();
}
