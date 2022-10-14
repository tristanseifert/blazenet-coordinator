#include <cairo.h>
#include <event2/event.h>
#include <fmt/format.h>

#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <linux/kernel.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

#include "Gui/DisplayManager.h"
#include "Gui/TextRenderer.h"
#include "Rpc/BlazedClient.h"
#include "Support/EventLoop.h"
#include "Support/Logging.h"

#include "Info.h"

using namespace Gui::Screens;

/**
 * @brief Per screen background colors
 *
 * Defines the key color for the background gradients
 */
const std::unordered_map<Info::Section, std::tuple<double, double, double>> Info::gBgColors{{
    { Section::Network,         {0., 0., .74}},
    { Section::BlazeNetStatus,  {.29, 0., .51}},
    { Section::BlazeNetTraffic, {.74, 0., 0.}},
    { Section::SystemStatus,    {.74, 0, .74}},
    { Section::Versions,        {.29, 0, .51}},
}};



/**
 * @brief Release drawing resources
 */
Info::~Info() {
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
        this->hasResources = true;
    }

    TextRenderer text(ctx);

    // create bg pattern and fill
    auto bgPattern = this->makeGradient(gBgColors.at(this->page));
    cairo_set_source(ctx, bgPattern);
    cairo_paint(ctx);
    cairo_pattern_destroy(bgPattern);

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
 * @brief Make background gradient
 */
cairo_pattern_t *Info::makeGradient(const std::tuple<double, double, double> &rgb) {
    auto pat = cairo_pattern_create_linear(120, 0, 120, 240.);
    if(cairo_pattern_status(pat) != CAIRO_STATUS_SUCCESS) {
        throw std::runtime_error(fmt::format("cairo_pattern_create_linear failed: {}",
                    cairo_pattern_status(pat)));
    }

    cairo_pattern_add_color_stop_rgb(pat, 0.,   0., 0., 0.);
    cairo_pattern_add_color_stop_rgb(pat, 0.6, 0., 0., 0.);

    const auto [r, g, b] = rgb;
    cairo_pattern_add_color_stop_rgb(pat, 1.0,  r, g, b);

    return pat;
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
        // redraw the page (or if cycling is temporarily disabled, re-render it always)
        if(this->page == Section::BlazeNetTraffic || this->page == Section::SystemStatus ||
            this->page == Section::Network || !this->cyclingEnabled) {
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

    // fetch the info
    std::string region{"???"};
    size_t channel{0}, numClients{0};
    double txPower{NAN};

    try {
        auto &rpc = Rpc::BlazedClient::The();
        rpc->getRadioConfig(region, channel, txPower);
        rpc->getClientStats(numClients);
    }
    // draw pretty errors
    catch(const std::exception &e) {
        DrawError(ctx, text, e.what());

        PLOG_ERROR << "failed to get BlazeNet status: " << e.what();
        return;
    }

    // left section (labels)
    text.setFont("DINish Condensed Bold", 18);
    text.draw(ctx, {0, 44}, {110, 32}, {1, 1, 1}, "Region:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 78}, {110, 32}, {1, 1, 1}, "Channel:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 112}, {110, 32}, {1, 1, 1}, "TX Power:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 146}, {110, 32}, {1, 1, 1}, "Clients:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    // right section (values)
    text.setFont("DINish", 18);

    text.draw(ctx, {115, 44}, {124, 32}, {1, 1, 1}, region,
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {115, 78}, {124, 32}, {1, 1, 1},
            fmt::format("<span font_features='tnum'>{}</span>", channel),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle, false, true);

    text.draw(ctx, {115, 112}, {124, 32}, {1, 1, 1},
            fmt::format("<span font_features='tnum'>{:.3} dBm</span>", txPower),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle, false, true);

    text.draw(ctx, {115, 146}, {124, 32}, {1, 1, 1},
            fmt::format("<span font_features='tnum'>{}</span>", numClients),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle, false, true);
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
    text.setFont("DINish Condensed Bold", 18);
    text.draw(ctx, {0, 44}, {110, 32}, {1, 1, 1}, "Load:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 78}, {110, 32}, {1, 1, 1}, "Uptime:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 112}, {110, 32}, {1, 1, 1}, "RAM:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    // values
    text.setFont("DINish", 18);

    // load averages
    const auto load1 = static_cast<double>(info.loads[0]) / static_cast<double>(1 << SI_LOAD_SHIFT);

    text.draw(ctx, {115, 44}, {124, 32}, {1, 1, 1}, fmt::format("{:4.2f}", load1),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle);

    // uptime
    std::stringstream upStr;
    upStr << "<span font_features='tnum'>";
    if(info.uptime >= (3600 * 24)) {
        size_t days = std::floor(info.uptime / (3600 / 24));
        upStr << days << "d ";
    }
    if(info.uptime >= 3600) {
        size_t hrs = std::fmod(std::floor(info.uptime / 3600), 24);
        upStr << hrs << "h ";
    }
    if(info.uptime >= 60 && info.uptime < (3600 * 24)) {
        // minutes only if uptime isn't > 1 day
        size_t mins = std::fmod(std::floor(info.uptime / 60), 60.);
        upStr << mins << "m ";
    }
    upStr << "</span>";

    text.draw(ctx, {115, 78}, {124, 32}, {1, 1, 1}, upStr.str(),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle, false, true);

    // memory usage
    const size_t bytesTotal = info.totalram * info.mem_unit,
          bytesFree = info.freeram * info.mem_unit,
          bytesUsed = bytesTotal - bytesFree;
    const double memPercent = (static_cast<double>(bytesUsed) / static_cast<double>(bytesTotal));

    text.draw(ctx, {115, 112}, {124, 32}, {1, 1, 1}, fmt::format("{:4.2f} %", memPercent * 100.),
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle);
}

/**
 * @brief Show the version page
 *
 * Show various software component versions, such as the kernel, OS release, and BlazeNet
 * coordinator.
 */
void Info::drawPageVersions(cairo_t *ctx, TextRenderer &text) {
    int err;

    DrawTitle(ctx, text, "Version");
    DrawFooter(ctx, text);

    // get blazed info
    std::string blazedVersion{""}, blazedBuild{""}, radioVersion{""};
    bool hasBlazedInfo{false};

    try {
        auto &rpc = Rpc::BlazedClient::The();
        rpc->getVersion(blazedVersion, blazedBuild, radioVersion);

        hasBlazedInfo = true;
    }
    // draw pretty errors
    catch(const std::exception &e) {
        PLOG_WARNING << "failed to get BlazeNet version: " << e.what();
        hasBlazedInfo = false;
    }
    // left section (labels)
    text.setFont("DINish Condensed Bold", 18);
    text.draw(ctx, {0, 44}, {110, 32}, {1, 1, 1}, "blazed:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 78}, {110, 32}, {1, 1, 1}, "Radio:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    text.draw(ctx, {0, 112}, {110, 32}, {1, 1, 1}, "Kernel:",
            TextRenderer::HorizontalAlign::Right, TextRenderer::VerticalAlign::Middle);

    // right section (values)
    text.setFont("DINish", 18);

    if(hasBlazedInfo) {
        text.draw(ctx, {115, 44}, {124, 32}, {1, 1, 1}, fmt::format("{} ({})", blazedVersion,
                    blazedBuild), TextRenderer::HorizontalAlign::Left,
                TextRenderer::VerticalAlign::Middle);

        text.draw(ctx, {115, 78}, {124, 32}, {1, 1, 1}, radioVersion,
                TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Middle);
    }

    // kernel version
    struct utsname kernelVers{};
    err = uname(&kernelVers);
    if(err == -1) {
        PLOG_WARNING << "uname failed: " << errno;
    } else {
        text.draw(ctx, {115, 112}, {124, 32}, {1, 1, 1}, fmt::format("{}", kernelVers.release),
                TextRenderer::HorizontalAlign::Left,
                TextRenderer::VerticalAlign::Middle);
    }
}



/**
 * @brief Draw an error message string
 */
void Info::DrawError(cairo_t *ctx, TextRenderer &text, const std::string_view &what) {
    // TODO: draw error icon (y = 44)

    // draw the error message
    text.setFont("Liberation Sans", 18);
    text.setTextLayoutWrapMode(true, true);
    text.setTextLayoutEllipsization(TextRenderer::EllipsizeMode::End);

    text.draw(ctx, {2, 78}, {236, 160}, {1, 1, 1}, what,
            TextRenderer::HorizontalAlign::Left, TextRenderer::VerticalAlign::Top);
}

/**
 * @brief Draw a screen title
 */
void Info::DrawTitle(cairo_t *ctx, TextRenderer &text, const std::string_view &title) {
    text.setFont("DINish Condensed Bold", 24);
    text.draw(ctx, {0, 0}, {240, 28}, {1, 1, 1}, title,
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
    text.draw(ctx, {0, 210}, {240, 28}, {.85, .85, .85}, str.str(),
            TextRenderer::HorizontalAlign::Center, TextRenderer::VerticalAlign::Top, false, true);
}
