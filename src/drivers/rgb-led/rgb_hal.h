#pragma once

#include <Arduino.h>
#include "app_config.h"

/*!
 * \file rgb_hal.h
 * \brief Hardware abstraction layer for RGB LED control
 * 
 * This class handles low-level RGB LED operations.
 * 
 * \note Thread-safe when used with proper mutex in higher-level services
 */

namespace PlantMonitor {
namespace Drivers {

/*!
 * \class RGBHal
 * \brief Hardware abstraction layer for RGB LED control
 * 
 * This class handles low-level RGB LED operations.
 * 
 * \note Thread-safe when used with proper mutex in higher-level services
 */

class RGBHal {
  public:
    /*!
     * \brief Constructor
     */
    RGBHal();

    /*!
     * \brief Destructor
     */
    ~RGBHal() = default;

    /*!
     * \brief Initialize RGB LED hardware
     * \return true if successful, false otherwise
     */
    bool begin();

    /*!
     * \brief Set RGB LED color
     * \param r Red component (0-255)
     * \param g Green component (0-255)
     * \param b Blue component (0-255)
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b);

    /*!
     * \brief Turn off RGB LED
     */
    void turnOff();

  private:
    uint8_t m_r_pin; //!< Red LED pin
    uint8_t m_g_pin; //!< Green LED pin
    uint8_t m_b_pin; //!< Blue LED pin

    // Prevent copying
    RGBHal(const RGBHal &) = delete;
};
} // namespace Drivers
} // namespace PlantMonitor