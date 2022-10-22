#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <cerrno>
#include <stdexcept>
#include <system_error>

#include <gpiod.h>
#include <fmt/format.h>
#include <TristLib/Core.h>

#include "Support/Gpio.h"

#include "St7789.h"

using namespace Drivers::Display;

/**
 * @brief Initialize the driver
 *
 * This configures the display driver based on the configuration specified. The following keys are
 * required to be present:
 *
 * - size: An array of [width, height] in pixels
 * - transport.device: SPI device file to use for communications
 * - transport.cs-gpio: GPIO line used for the display chip select (managed in software)
 * - transport.reset-gpio: GPIO line used for the display /RESET signal
 * - transport.dc-gpio: GPIO line used for the display D/C signal
 *
 * The following keys are optional:
 * - rotation: Display rotation, in degrees: one of [0, 90, 180, 270] where 0 is the default
 * - transport.max-speed: Maximum frequency to use for SPI communications (default 1MHz)
 * - backlight.path: Path to a backlight file
 */
St7789::St7789(const toml::table &config) : Base(config) {
    this->initSize(config);
    this->initSpidev(config);
    this->initGpios(config);
    // TODO: backlight

    // perform display initialization
    this->reset();
    this->initDisplay();
}

/**
 * @brief Initialize display size and rotation
 *
 * Read out the size (stored as an array) and optional rotation from the display config.
 */
void St7789::initSize(const toml::table &config) {
    const auto &size = config.at_path("size");
    if(!size || !size.is_array()) {
        throw std::runtime_error("invalid display size (expected array)");
    } else {
        auto &arr = *size.as_array();
        if(arr.size() != 2) {
            throw std::runtime_error(fmt::format(
                        "invalid display size (expected 2 elements, got {})", arr.size()));
        }

        this->width = arr[0].value_or(0);
        this->height = arr[1].value_or(0);

        if(!this->width || !this->height) {
            throw std::runtime_error(fmt::format("invalid size ({} x {})", this->width,
                        this->height));
        }
    }

    // allocate the framebuffer
    this->buffer.resize(this->width * 2 * this->height);
    this->drawTestPattern();
}

/**
 * @brief Open SPI device
 *
 * Open the SPI device that's used to communicate with the display, then configure it for 8-bit
 * communication with the specified maximum frequency.
 */
void St7789::initSpidev(const toml::table &config) {
    int err;

    // get the max baud rate
    const uint32_t speed = config.at_path("transport.max-speed").value_or(kDefaultSpiRate);
    PLOG_DEBUG << "SPI rate: " << speed / 1000. << " kHz";

    // open device
    const std::string path = config.at_path("transport.device").value_or("(null)");
    PLOG_DEBUG << "Display device: " << path;

    err = open(path.c_str(), O_RDWR);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(),
                fmt::format("open spidev ({})", path));
    }

    this->spiFd = err;

    // configure device: mode 0 with 8 bits per word
    uint8_t mode{SPI_MODE_0};
    err = ioctl(this->spiFd, SPI_IOC_WR_MODE, &mode);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "configure spidev mode");
    }

    uint8_t bits{0};
    err = ioctl(this->spiFd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "configure spidev bits per word");
    }

    // also, set the maximum baud rate
    err = ioctl(this->spiFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "configure spidev speed");
    }
}

/**
 * @brief Initialize GPIOs
 *
 * Open and request as outputs the display reset and D/C lines.
 */
void St7789::initGpios(const toml::table &config) {
    int err;

    // reset GPIO
    try {
        const std::string line = config.at_path("transport.reset-gpio").value_or("");
        this->gpioReset = Support::Gpio::GetLine(line);

        err = gpiod_line_request_output(this->gpioReset, "blazeguid-st7789-reset", 1);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "request output");
        }
    } catch(const std::exception &e) {
        throw std::runtime_error(fmt::format("failed to init reset gpio: {}", e.what()));
    }

    // data/command line
    try {
        const std::string line = config.at_path("transport.dc-gpio").value_or("");
        this->gpioDataCmd = Support::Gpio::GetLine(line);

        err = gpiod_line_request_output(this->gpioDataCmd, "blazeguid-st7789-dc", 1);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "request output");
        }
    } catch(const std::exception &e) {
        throw std::runtime_error(fmt::format("failed to init data/cmd gpio: {}", e.what()));
    }

    // chip select
    try {
        const std::string line = config.at_path("transport.cs-gpio").value_or("");
        this->gpioSelect = Support::Gpio::GetLine(line);

        err = gpiod_line_request_output(this->gpioSelect, "blazeguid-st7789-cs", 1);
        if(err) {
            throw std::system_error(errno, std::generic_category(), "request output");
        }
    } catch(const std::exception &e) {
        throw std::runtime_error(fmt::format("failed to init chip select gpio: {}", e.what()));
    }
}

/**
 * @brief Clean up display resources
 *
 * Reset the display to clear the display and turn off the backlight
 */
St7789::~St7789() {
    // reset display
    this->reset(false);

    // shut off backlight
    if(this->backlightFd != -1) {
        this->setBacklight(0);
        close(this->backlightFd);
    }

    // release the spidev
    if(this->spiFd != -1) {
        close(this->spiFd);
    }

    // release IOs
    gpiod_line_close_chip(this->gpioReset);
    gpiod_line_close_chip(this->gpioDataCmd);

    if(this->gpioSelect) {
        gpiod_line_close_chip(this->gpioSelect);
    }
}



/**
 * @brief Reset the display
 *
 * Following section 7.4.5 timings, we'll assert the reset line low for 1ms, then wait 120ms (the
 * maximum case, if the display is in sleep mode) for the display to be ready.
 *
 * @param wait Whether to wait after resetting the display
 */
void St7789::reset(const bool wait) {
    // de-assert chip select and command strobe
    Support::Gpio::SetState(this->gpioSelect, 1, "deassert chip select");
    Support::Gpio::SetState(this->gpioDataCmd, 1, "assert data");

    // assert reset
    Support::Gpio::SetState(this->gpioReset, 0, "assert reset");
    // wait
    usleep(1000 * 1);
    // deassert reset
    Support::Gpio::SetState(this->gpioReset, 1, "deassert reset");

    // wait some more
    if(wait) {
        usleep(1000 * 120);
    }
}

/**
 * @brief Send a command to display
 *
 * Write a command to the display, which may be followed by multiple data bytes.
 */
void St7789::sendCommand(const uint8_t cmd, std::span<const uint8_t> payload) {
    int err;

    // assert chip select
    Support::Gpio::SetState(this->gpioSelect, 0, "assert chip select");

    // write command byte
    Support::Gpio::SetState(this->gpioDataCmd, 0, "assert command");

    err = write(this->spiFd, &cmd, sizeof(cmd));
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), "write command");
    }

    // write payload, if any
    if(!payload.empty()) {
        Support::Gpio::SetState(this->gpioDataCmd, 1, "assert data");

        // split the payload into 4K chunks
        // XXX: can we make this better if the SPI driver supports larger transfers?
        constexpr static const size_t kChunkSize{4096};
        size_t offset{0};
        while(offset < payload.size()) {
            // get this chunk
            auto chunk = payload.subspan(offset, std::min(payload.size() - offset, kChunkSize));

            // write it
            err = write(this->spiFd, chunk.data(), chunk.size());
            if(err == -1) {
                throw std::system_error(errno, std::generic_category(), "write payload");
            } else if(err != chunk.size()) {
                throw std::runtime_error(fmt::format("partial data write: {} of {}", err,
                            chunk.size()));
            }

            // update for next transfer
            offset += chunk.size();
        }
    }

    // de-assert chip select
    Support::Gpio::SetState(this->gpioSelect, 1, "deassert chip select");
}

/**
 * @brief Perform display initialization sequence
 */
void St7789::initDisplay() {
    // Memory data access control (based on rotation)
    this->sendCommand(0x36, {{0x00}}); // or 0x70
    // Interface pixel format: 16bpp
    this->sendCommand(Command::PixelFormat, {{0x05}});
    // Porch control
    this->sendCommand(0xB2, {{0x0C, 0x0C, 0x00, 0x33, 0x33}});
    // Gate control
    this->sendCommand(0xB7, {{0x35}});
    // VCOM setting
    this->sendCommand(0xBB, {{0x19}});
    // LCM Control
    this->sendCommand(0xC0, {{0x2C}});
    // VDV and VRH command enable
    this->sendCommand(0xC2, {{0x01}});
    // VRH set
    this->sendCommand(0xC3, {{0x12}});
    // VDV set
    this->sendCommand(0xC4, {{0x20}});
    // Frame rate control
    this->sendCommand(0xC6, {{0x0F}});
    // Power control 1
    this->sendCommand(0xD0, {{0xA4, 0xA1}});
    // Positive voltage gamma
    this->sendCommand(Command::GammaPos, {{0XD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54,
            0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}});
    // Negative voltage gamma
    this->sendCommand(Command::GammaNeg, {{0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44,
            0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}});

    // display inversion off
    this->sendCommand(Command::InvertOn);

    // sleep out
    this->sendCommand(Command::SleepOut);
    usleep(120 * 1000);

    // transfer display buffer (so there's something reasonable in it)
    this->transferBuffer();
}

/**
 * @brief Set the display position
 *
 * This is used for the next "memory write" command.
 */
void St7789::setPosition(const std::pair<uint16_t, uint16_t> &start,
        const std::pair<uint16_t, uint16_t> &end) {
    this->sendCommand(Command::ColumnAddrSet, {{
        static_cast<uint8_t>(start.first >> 8), static_cast<uint8_t>(start.first & 0xFF),
        static_cast<uint8_t>(end.first >> 8),   static_cast<uint8_t>(end.first & 0xFF),
    }});

    this->sendCommand(Command::RowAddrSet, {{
        static_cast<uint8_t>(start.second >> 8), static_cast<uint8_t>(start.second & 0xFF),
        static_cast<uint8_t>(end.second >> 8),   static_cast<uint8_t>(end.second & 0xFF),
    }});
}



/**
 * @brief Draw a test pattern
 *
 * This has red, green, blue, and then a grey color gradient from top to bottom.
 */
void St7789::drawTestPattern() {
    uint16_t temp;

    for(size_t y = 0; y < this->height; y++) {
        const size_t yOff = y * this->getFramebufferStride();

        for(size_t x = 0; x < this->width; x++) {
            // red
            if(y < 60) {
                temp = ((x >> 2) & 0x1f) << 11;
            }
            // grün
            else if(y >= 60 && y < 120) {
                temp = ((x >> 1) & 0x3f) << 5;
            }
            // blau
            else if(y >= 120 && y < 180) {
                temp = ((x >> 2) & 0x1f) << 0;
            }
            // grey
            else {
                temp = (((x >> 2) & 0x1f) << 11) | (((x >> 1) & 0x3f) << 5) |
                    (((x >> 2) & 0x1f) << 0);

            }

            this->buffer[yOff + (x * 2) + 0] = temp >> 8;
            this->buffer[yOff + (x * 2) + 1] = temp & 0xff;
        }
    }
}
