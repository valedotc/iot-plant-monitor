#pragma once

#include <Arduino.h>

/*!
 * \file button-sensor-hal.cpp
 * \brief Hardware Abstraction Layer for Button Sensor
 */

#define DEBOUNCE_DELAY 200 //!< Debounce delay in milliseconds

namespace PlantMonitor {
namespace Drivers {

/*!
 * \enum ButtonInputMode
 * \brief Enumeration for button input modes
 */
enum ButtonInputMode {
    BUTTON_INPUT_PULLUP = INPUT_PULLUP,
    BUTTON_INPUT_PULLDOWN = INPUT_PULLDOWN
};

/*!
 * \class ButtonHal
 * \brief Hardware Abstraction Layer for a button sensor
 */
class ButtonHal {
  public:
    /*!
        * \brief Constructor for ButtonHal
        * \param pin GPIO pin number where the button is connected
        * \param mode Input mode for the button (pull-up or pull-down)
        * \param interruptHandler Optional interrupt handler function to be called on button press
        */
    explicit ButtonHal(uint8_t pin, enum ButtonInputMode mode = BUTTON_INPUT_PULLUP, void (*handler)() = nullptr);

    /*!
        * \brief Check if the button press is debounced
        * \return true if debounced, false otherwise
        */
    bool debouncing();

    /*!
        * \brief Get the GPIO pin number
        * \return GPIO pin number
        */
    uint8_t getPin() const;

  private:
    uint8_t pin;                       //!< GPIO pin number
    void (*interruptHandler)();        //!< Interrupt handler function
    unsigned long int lastIntTime = 0; //!< Last interrupt time for debouncing
};

} // namespace Drivers
} // namespace PlantMonitor