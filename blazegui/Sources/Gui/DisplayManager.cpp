#include <cairo.h>

#include <fmt/format.h>

#include <stdexcept>

#include "Drivers/Display/Base.h"
#include "Support/Logging.h"

#include "DisplayManager.h"

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
    cairo_set_source_rgb(this->ctx, 0.33, 0.33, 1);
    cairo_paint(this->ctx);
}

/**
 * @brief Clean up drawing context
 */
DisplayManager::~DisplayManager() {
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
    if(!force && !this->dirty) {
        return;
    }

    // prepare context
    cairo_save(this->ctx);

    // draw
    // TODO: this

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
