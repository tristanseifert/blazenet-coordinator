#ifndef DRIVERS_DISPLAY_BASE_H
#define DRIVERS_DISPLAY_BASE_H

#include <cmath>
#include <stdexcept>

#include <toml++/toml.h>

/// Graphics display drivers
namespace Drivers::Display {
/**
 * @brief Display driver base class
 *
 * All display drivers should inherit from this base class.
 */
class Base {
    public:
        /**
         * @brief Initialize the driver
         *
         * Set up the driver, which would perform some hardware initialization as well
         */
        Base(const toml::table &config) {};

        /**
         * @brief Clean up driver
         *
         * Release any allocated resources (such as display buffers) and turn off the display, if
         * supported.
         */
        virtual ~Base() = default;


        /**
         * @brief Reset the display controller
         */
        virtual void reset() = 0;

        /// @{ @name Dimensions
        /**
         * @brief Get the width of the display
         *
         * @return Display width, in pixels
         */
        virtual size_t getWidth() const = 0;

        /**
         * @brief Get the height of the display
         *
         * @return Display height, in pixels
         */
        virtual size_t getHeight() const = 0;
        /// @}

        /// @{ @name Backlight
        /**
         * @brief Can the brightness of the display backlight be adjusted?
         *
         * Some displays have software controllable backlights, which may be controlled via this
         * driver interface.
         */
        virtual bool supportsBacklightAdjust() const = 0;

        /**
         * @brief Get the current display brightness
         *
         * @return Display brightness, in the range [0, 1]
         */
        virtual float getBacklight() const {
            return NAN;
        }

        /**
         * @brief Set the display brightness
         *
         * @param brightness New brightness value, in the range [0, 1]
         */
        virtual void setBacklight(const float brightness) {
            throw std::logic_error("not supported");
        }
        /// @}
};
}

#endif
