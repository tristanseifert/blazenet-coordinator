#ifndef DRIVERS_DISPLAY_ST7789_H
#define DRIVERS_DISPLAY_ST7789_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "Base.h"

namespace Drivers::Display {
/**
 * @brief LCD driver for ST7789-based displays
 *
 * This is a basic driver that communicates with an ST7789-based display via SPI.
 */
class St7789: public Base {
    private:
        enum Command: uint8_t {
            SleepOut                    = 0x11,
            InvertOff                   = 0x20,
            InvertOn                    = 0x21,
            DisplayOff                  = 0x28,
            DisplayOn                   = 0x29,
            ColumnAddrSet               = 0x2A,
            RowAddrSet                  = 0x2B,
            WriteVRAM                   = 0x2C,
            PixelFormat                 = 0x3A,
            GammaPos                    = 0xE0,
            GammaNeg                    = 0xE1,
        };

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

        /**
         * Send the display on or display off command, depending on the flag.
         */
        void setEnabled(const bool enable) override {
            if(enable) {
                this->sendCommand(Command::DisplayOn);
            } else {
                this->sendCommand(Command::DisplayOff);
            }
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

        void transferBuffer() override {
            this->writeVram({0, 0}, {this->width, this->height}, this->getFramebuffer());
        }
        /// Framebuffer is stored as a vector (allocated once size is known)
        std::span<uint8_t> getFramebuffer() override {
            return this->buffer;
        }
        /// We always use a 16bpp framebuffer
        size_t getBitsPerPixel() const override {
            return 16;
        }
        /// Our buffer will always be tightly packed
        size_t getFramebufferStride() const override {
            return this->width * 2;
        }

    private:
        void initSize(const toml::table &);
        void initSpidev(const toml::table &);
        void initGpios(const toml::table &);

        void reset(const bool wait);
        void sendCommand(const uint8_t cmd, std::span<const uint8_t> payload = {});

        void initDisplay();

        void setPosition(const std::pair<uint16_t, uint16_t> &start,
                const std::pair<uint16_t, uint16_t> &end);

        /**
         * @brief Write to VRAM
         *
         * Data is written to the previously set location.
         *
         * @param buffer Data to write to VRAM
         */
        inline void writeVram(std::span<const uint8_t> buffer) {
            this->sendCommand(Command::WriteVRAM, buffer);
        }
        /**
         * @brief Write to VRAM
         *
         * Write the data to the specified window in VRAM.
         *
         * @param start Starting position of data write
         * @param end Ending position of data write
         * @param buffer Data to write to VRAM
         */
        inline void writeVram(const std::pair<uint16_t, uint16_t> &start,
                const std::pair<uint16_t, uint16_t> &end, std::span<const uint8_t> buffer) {
            this->setPosition(start, end);
            this->writeVram(buffer);
        }

        void drawTestPattern();

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

        /// Framebuffer memory
        std::vector<uint8_t> buffer;
};
};

#endif
