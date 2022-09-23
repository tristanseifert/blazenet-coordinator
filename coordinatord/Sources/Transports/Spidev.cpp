#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <array>
#include <cerrno>
#include <regex>
#include <system_error>

#include <gpiod.h>
#include <event2/event.h>
#include <event2/util.h>
#include <fmt/format.h>

#include "Support/EventLoop.h"
#include "Support/Logging.h"

#include "Transports/Spidev.h"

using namespace Transports;

/**
 * @brief Initialize the SPI transport
 *
 * @param config Contents of the `radio.transport` table in the config
 */
Spidev::Spidev(const toml::table &config) {
    // open the spidev first
    this->openSpidev(config);

    // set up irq line
    auto irq = config["irq"];
    if(!irq || !irq.is_string()) {
        throw std::runtime_error("invalid or missing `radio.transport.irq` key");
    }

    this->initIrq(irq.value_or(""));

    // set up reset line, if specified
    auto reset = config["reset"];
    if(reset && reset.is_string()) {
        this->initReset(reset.value_or(""));
    } else if(reset) {
        throw std::runtime_error("invalid `radio.transport.reset` key");
    }
}

/**
 * @brief Open and configure the SPI device
 *
 * Read the configuration blob specified to determine what SPI device to open, and how to configure
 * the device.
 *
 * The following keys are mandatory:
 *
 * - file: Device file path to open
 * - freq: Transfer frequency to use
 * - mode: SPI mode (0-3) to apply
 */
void Spidev::openSpidev(const toml::table &config) {
    int err;

    // validate config
    if(!config.contains("file") || !config["file"].is_string()) {
        throw std::runtime_error("invalid or missing `radio.transport.file` key (expected string)");
    } else if(!config.contains("freq") || !config["freq"].is_integer()) {
        throw std::runtime_error("invalid or missing `radio.transport.freq` key (expected int)");
    } else if(!config.contains("mode") || !config["mode"].is_integer()) {
        throw std::runtime_error("invalid or missing `radio.transport.mode` key (expected int)");
    }

    const uint32_t freq = **(config["freq"].as_integer()),
          mode = **(config["mode"].as_integer());
    if(mode > 3) {
        throw std::runtime_error("invalid `radio.transport.mode` (must be [0, 3])");
    }

    // open device and configure it
    const std::string spidevPath = config["file"].value_or("");

    this->spidev = ::open(spidevPath.c_str(), O_RDWR);
    if(this->spidev == -1) {
        throw std::system_error(errno, std::generic_category(), "failed to open spidev");
    }
    PLOG_VERBOSE << "Opened spidev: " << spidevPath;

    static const std::array<size_t, 4> kSpiModes{{
        SPI_MODE_0, SPI_MODE_1, SPI_MODE_2, SPI_MODE_3
    }};

    uint8_t modeTemp = static_cast<uint8_t>(kSpiModes.at(mode));

    err = ioctl(this->spidev, SPI_IOC_WR_MODE, &modeTemp);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "set spidev mode");
    }

    uint8_t bitsPerWord{8};
    err = ioctl(this->spidev, SPI_IOC_WR_BITS_PER_WORD, &bitsPerWord);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "set spidev bits per word");
    }

    err = ioctl(this->spidev, SPI_IOC_WR_MAX_SPEED_HZ, &freq);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "set spidev frequency");
    }
}

/**
 * @brief Initialize the IRQ line
 *
 * Set up an interrupt watch on the specified IO line.
 *
 * @param lineDesc IO line description, in the format of `gpiochip:pin`
 */
void Spidev::initIrq(const std::string &lineDesc) {
    int err, eventFd;
    const auto [chipName, pin] = ParseGpio(lineDesc);

    PLOG_DEBUG << "IRQ line: " << fmt::format("{} (chip '{}', line {})", lineDesc, chipName, pin);

    // get the IO line
    auto line = gpiod_line_get(chipName.c_str(), pin);
    if(!line) {
        throw std::system_error(errno, std::generic_category(),
                fmt::format("failed to get irq ({})", lineDesc));
    }

    // get falling edge interrupts
    err = gpiod_line_request_falling_edge_events(line, "blazed-spidev-irq");
    if(err) {
        gpiod_line_close_chip(line);
        throw std::system_error(errno, std::generic_category(),
                fmt::format("reserve irq input ({})", lineDesc));
    }

    // set up event source
    eventFd = gpiod_line_event_get_fd(line);
    if(eventFd == -1) {
        gpiod_line_close_chip(line);
        throw std::system_error(errno, std::generic_category(),
                fmt::format("get irq events fd ({})", lineDesc));
    }

    err = evutil_make_socket_nonblocking(eventFd);
    if(err) {
        gpiod_line_close_chip(line);
        throw std::system_error(errno, std::generic_category(),
                fmt::format("make irq events fd nonblocking ({})", lineDesc));
    }

    auto evbase = Support::EventLoop::Current()->getEvBase();
    auto ev = event_new(evbase, eventFd, EV_READ | EV_PERSIST, [](auto fd, auto ev, auto ctx) {
        reinterpret_cast<Spidev *>(ctx)->handleIrq(fd, ev);
    }, this);

    err = event_add(ev, nullptr);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "register irq events");
    }


    this->irqLine = line;
    this->irqLineEvent = ev;
}

/**
 * @brief Initialize the reset line
 *
 * Configure the line as an open drain output.
 *
 * @paramDesc line IO line description, in the format of `gpiochip:pin`
 */
void Spidev::initReset(const std::string &lineDesc) {
    int err;
    const auto [chipName, pin] = ParseGpio(lineDesc);

    PLOG_DEBUG << "Reset line: " << fmt::format("{} (chip '{}', line {})", lineDesc, chipName, pin);

    // get the IO line
    auto line = gpiod_line_get(chipName.c_str(), pin);
    if(!line) {
        throw std::system_error(errno, std::generic_category(),
                fmt::format("failed to get reset ({})", lineDesc));
    }

    // request as output
    err = gpiod_line_request_output_flags(line, "blazed-spidev-reset",
            GPIOD_LINE_REQUEST_FLAG_OPEN_DRAIN | GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP |
            GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW,
            false);
    if(err) {
        throw std::system_error(errno, std::generic_category(),
            fmt::format("request reset line ({})", lineDesc));
    }

    this->resetLine = line;
}

/**
 * @brief Release all resources
 *
 * Close the SPI device and release GPIOs.
 */
Spidev::~Spidev() {
    // remove event sources
    if(this->irqLineEvent) {
        event_del(this->irqLineEvent);
        event_free(this->irqLineEvent);
        this->irqLineEvent = nullptr;
    }

    // close GPIOs
    if(this->irqLine) {
        gpiod_line_close_chip(this->irqLine);
        this->irqLine = nullptr;
    }
    if(this->resetLine) {
        gpiod_line_close_chip(this->resetLine);
        this->irqLine = nullptr;
    }

    // close spidev
    if(this->spidev > 0) {
        ::close(this->spidev);
        this->spidev = 0;
    }
}



/**
 * @brief Assert the reset line
 *
 * This toggles the reset line low for ~20ms.
 */
void Spidev::reset() {
    int err;
    if(!this->resetLine) {
        return;
    }

    // assert reset
    err = gpiod_line_set_value(this->resetLine, true);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "assert reset line");
    }

    // wait for reset to complete
    usleep(20 * 1000);

    // then release (deasert) the line
    err = gpiod_line_set_value(this->resetLine, false);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "deassert reset line");
    }

    /*
     * Wait for the controller to boot up
     *
     * This may take significantly longer (~30 sec) if the external flash needs formatting, or some
     * other long-running maintenance operation is ongoing. We assume those take place while the
     * Linux system we're running on is booting.
     */
    usleep(250 * 1000);
}

/**
 * @brief Send a command, then read response
 *
 * Set up an SPI transaction to transmit the given command, then read the number of bytes specified
 * (by the buffer size) from the controller.
 *
 * @param command Command id
 * @param buffer Buffer to receive the response
 */
void Spidev::sendCommandWithResponse(const CommandId command, std::span<std::byte> buffer) {
    int err;

    // validate args and build command
    if(buffer.empty()) {
        throw std::invalid_argument("buffer empty");
    } else if(buffer.size() > UINT8_MAX) {
        throw std::invalid_argument("buffer too long");
    } else if(static_cast<size_t>(command) > 0x7F) {
        throw std::invalid_argument("invalid command id");
    }

    const uint8_t rawCmd = static_cast<uint8_t>(command) | 0x80;
    CommandHeader cmd{rawCmd, static_cast<uint8_t>(buffer.size())};

    // build request structure
    std::array<struct spi_ioc_transfer, 2> transfers{{
        {
            .tx_buf = reinterpret_cast<unsigned long>(&cmd),
            .rx_buf = 0,
            .len = sizeof(cmd),
            .delay_usecs = kReadCmdDelay,
        },
        {
            .tx_buf = 0,
            .rx_buf = reinterpret_cast<unsigned long>(buffer.data()),
            .len = buffer.size(),
        }
    }};

    err = ioctl(this->spidev, SPI_IOC_MESSAGE(2), transfers.data());
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), __func__);
    }
}

/**
 * @brief Send the given command with payload
 *
 * Set up an SPI transaction to send the given command, followed immediately by the given payload
 * bytes.
 *
 * @param command Command id
 * @param payload Payload data to send immediately after
 */
void Spidev::sendCommandWithPayload(const CommandId command, std::span<const std::byte> payload) {
    int err;

    // validate args and build command
    if(payload.size() > UINT8_MAX) {
        throw std::invalid_argument("payload too long");
    } else if(static_cast<size_t>(command) > 0x7F) {
        throw std::invalid_argument("invalid command id");
    }

    CommandHeader cmd{static_cast<uint8_t>(command), static_cast<uint8_t>(payload.size())};

    // build request structure
    std::array<struct spi_ioc_transfer, 2> transfers{{
        {
            .tx_buf = reinterpret_cast<unsigned long>(&cmd),
            .rx_buf = 0,
            .len = sizeof(cmd),
            .delay_usecs = kWriteCmdDelay,
        },
        {
            .tx_buf = reinterpret_cast<unsigned long>(payload.data()),
            .rx_buf = 0,
            .len = payload.size(),
        }
    }};

    err = ioctl(this->spidev, SPI_IOC_MESSAGE(payload.empty() ? 1 : 2), transfers.data());
    if(err == -1) {
        throw std::system_error(errno, std::generic_category(), __func__);
    }
}



/**
 * @brief Handle a change on the interrupt line
 *
 * Reads the next GPIO event from the descriptor and acts on it.
 *
 * @param fd File descriptor for the IRQ line event source
 * @param flags libevent flags (should be EV_READ)
 */
void Spidev::handleIrq(int fd, size_t flags) {
    int err;
    struct gpiod_line_event info;

    err = gpiod_line_event_read_fd(fd, &info);
    if(err) {
        throw std::system_error(errno, std::generic_category(), "read irq gpio event");
    }

    // process the event
    PLOG_DEBUG << "Event type: " << info.event_type;

    if(info.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
        this->invokeIrqHandlers();
    }
}



/**
 * @brief Parse a GPIO descriptor
 *
 * This is a string in the format of `gpiochip:pin`, where `pin` is an integer.
 *
 * @param in Input string to parse
 * @return A pair of GPIP chip name and pin number
 */
std::pair<std::string, size_t> Spidev::ParseGpio(const std::string &in) {
    static const std::regex kRegex("(\\w+)(?::)(\\d+)");
    std::smatch match;

    if(!std::regex_match(in, match, kRegex)) {
        throw std::runtime_error(fmt::format("invalid gpio descriptor: `{}`", in));
    }

    return std::make_pair(match[1].str(), std::stol(match[2].str()));
}
