#ifndef DRIVERS_DISPLAY_ST7789_H
#define DRIVERS_DISPLAY_ST7789_H

#include <cstddef>
#include <cstdint>
#include <span>

#include "Base.h"

namespace Drivers::Display {
/**
 * @brief LCD driver for ST7789-based displays
 *
 * This is a basic driver that communicates with an ST7789-based display via SPI.
 */
class St7789: public Base {
    private:
        /// Default SPI frequency (in Hz)
        constexpr static const size_t kDefaultSpiRate{1'000'000};

    public:
        St7789(const toml::table &config);
        ~St7789();

    public:
        void reset() override {
            this->reset(true);
        }

        size_t getWidth() const override {
            return this->width;
        }
        size_t getHeight() const override {
            return this->height;
        }

        /**
         * @brief We can control a Linux PWM backlight instance, if opened.
         */
        bool supportsBacklightAdjust() const override {
            return (this->backlightFd != -1);
        }

    private:
        void initSize(const toml::table &);
        void initSpidev(const toml::table &);
        void initGpios(const toml::table &);

        void reset(const bool wait);
        void sendCommand(const uint8_t cmd, std::span<const uint8_t> payload = {});

        void initDisplay();

    private:
        /// Pixel dimensions of the display
        size_t width{0}, height{0};
        /// Rotation of the display
        int rotation{0};

        /// File descriptor for SPI device
        int spiFd{-1};

        /// File descriptor for the backlight file
        int backlightFd{-1};
        /// Last written backlight level
        float backlightLevel{0.};

        /// Chip select GPIO
        struct gpiod_line *gpioSelect{nullptr};
        /// Reset GPIO
        struct gpiod_line *gpioReset{nullptr};
        /// D/C GPIO
        struct gpiod_line *gpioDataCmd{nullptr};
};
};

#endif
