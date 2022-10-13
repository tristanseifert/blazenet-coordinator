#include <cairo.h>
#include <event2/event.h>
#include <fmt/format.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <linux/kernel.h>
#include <sys/sysinfo.h>

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
        reinterpret_cast<Info *>(ctx)->timerFired();
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

        case Section::BlazeNetStatus:
            this->drawPageBlazeStatus(ctx, text);
            break;

        case Section::BlazeNetTraffic:
            this->drawPageBlazeTraffic(ctx, text);
            break;

        case Section::SystemStatus:
            this->drawPageSysStatus(ctx, text);
            break;

        case Section::Versions:
            this->drawPageVersions(ctx, text);
            break;

        default:
            throw std::runtime_error("invalid page");
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
 * @brief Timer has fired
 */
void Info::timerFired() {
    // flip page if needed
    if(this->cyclingEnabled && this->pageCycles++ >= kPageFlipCycles) {
        this->flipPage();
        this->pageCycles = 0;
    }
    // otherwise, re-render
    else {
        // redraw the page
        if(this->page == Section::BlazeNetTraffic || this->page == Section::SystemStatus) {
            this->dirtyFlag = true;
        }
    }
}

/**
 * @brief Flip the displayed information page
 */
void Info::flipPage() {
    // get next page
    size_t temp = static_cast<size_t>(this->page) + 1;
    if(temp > static_cast<size_t>(Section::Last)) {
        temp = 0;
    }
    this->page = static_cast<Section>(temp);

    // mark as dirty
    this->dirtyFlag = true;
}



/**
 * @brief Draw the network page
 *
 * Show the IP addresses of active network interfaces.
 */
void Info::drawPageNetwork(cairo_t *ctx, TextRenderer &text) {
    DrawTitle(ctx, text, "IP Status");
    DrawFooter(ctx, text);

    // TODO: implement
}

/**
 * @brief Draw the BlazeNet status page
 *
 * Show the IP addresses of active network interfaces.
 */
void Info::drawPageBlazeStatus(cairo_t *ctx, TextRenderer &text) {
    DrawTitle(ctx, text, "BlazeNet Status");
    DrawFooter(ctx, text);

    // TODO: implement
}

/**
 * @brief Draw the BlazeNet traffic page
 *
 * Show the current rate of traffic (receive and transmit) over the BlazeNet radio.
 */
void Info::drawPageBlazeTraffic(cairo_t *ctx, TextRenderer &text) {
    DrawTitle(ctx, text, "BlazeNet Traffic");
    DrawFooter(ctx, text);

    // TODO: implement
}

/**
 * @brief Draw the system status page
 *
 * This page shows the system's uptime, load average, memory usage, and other such fun
 * information.
 */
void Info::drawPageSysStatus(cairo_t *ctx, TextRenderer &text) {
    int err;
    struct sysinfo info{};

    DrawTitle(ctx, text, "System Status");
    DrawFooter(ctx, text);

    // get sys info
    err = sysinfo(&info);
    if(err) {
        PLOG_WARNING << "sysinfo failed: " << err << "(errno = " << errno << ")";

        // draw a cross
        cairo_set_source_rgb(ctx, 1, 0, 0);
        cairo_set_line_width(ctx, 5.);

        cairo_move_to(ctx, 20, 20);
        cairo_line_to(ctx, 220, 220);
        cairo_stroke(ctx);

        cairo_move_to(ctx, 220, 20);
        cairo_move_to(ctx, 20, 220);
        cairo_stroke(ctx);
        return;
    }

    // left section (labels)
    text.setFont("DINish Condensed Bold", 20);
    text.draw(ctx, {0, 44}, {100, 32}, {1, 1, 1}, "Load:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 78}, {100, 32}, {1, 1, 1}, "Uptime:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 112}, {100, 32}, {1, 1, 1}, "RAM:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    // values
    text.setFont("DINish", 20);

    // load averages
    const auto load1 = static_cast<double>(info.loads[0]) / static_cast<double>(1 << SI_LOAD_SHIFT);

    text.draw(ctx, {105, 44}, {134, 32}, {1, 1, 1}, fmt::format("{:4.2f}", load1),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle);

    // uptime
    text.draw(ctx, {105, 78}, {134, 32}, {1, 1, 1}, fmt::format("{}", info.uptime),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle);

    // memory usage
    const size_t bytesTotal = info.totalram * info.mem_unit,
          bytesFree = info.freeram * info.mem_unit,
          bytesUsed = bytesTotal - bytesFree;
    const double memPercent = (static_cast<double>(bytesUsed) / static_cast<double>(bytesTotal));

    text.draw(ctx, {105, 112}, {134, 32}, {1, 1, 1}, fmt::format("{:4.2f} %", memPercent * 100.),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle);
}

/**
 * @brief Show the version page
 *
 * Show various software component versions, such as the kernel, OS release, and BlazeNet
 * coordinator.
 */
void Info::drawPageVersions(cairo_t *ctx, TextRenderer &text) {
    DrawTitle(ctx, text, "Version");
    DrawFooter(ctx, text);

    // TODO: implement
}



/**
 * @brief Draw a screen title
 */
void Info::DrawTitle(cairo_t *ctx, TextRenderer &text, const std::string_view &str) {
    text.setFont("DINish Condensed Bold", 24);
    text.draw(ctx, {0, 0}, {240, 28}, {1, 1, 1}, str,
            TextRenderer::HorizontalAlign::Center, TextRenderer::VerticalAlign::Top);
}

/**
 * @brief Draw the footer with the current date and time
 */
void Info::DrawFooter(cairo_t *ctx, TextRenderer &text) {
    // format the time
    std::stringstream str;
    std::time_t t = std::time(nullptr);
    std::time(&t);

    str << "<span font_features='tnum'>";
    str << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M");
    str << "</span>";

    // render it
    text.setFont("DINish", 18);
    text.draw(ctx, {0, 212}, {240, 28}, {.85, .85, .85}, str.str(),
            TextRenderer::HorizontalAlign::Center, TextRenderer::VerticalAlign::Top, false, true);
}
