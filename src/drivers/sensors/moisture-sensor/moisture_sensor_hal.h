#pragma once

#include <Arduino.h>
#include <functional>
#include "app_config.h"

namespace PlantMonitor {
namespace Drivers {

/*!
 * \brief Hardware Abstraction Layer for the capacitive soil moisture sensor (v1.2)
 *
 * Supports optional mockable analogRead function for unit testing.
 */
class MoistureSensorHAL {
  public:
    /*!
     * \brief Type alias for a custom analog read function
     */
    using AnalogReader = std::function<int(uint8_t)>;

    /*!
     * \brief Constructor
     * \param dryValue ADC value representing completely dry soil
     * \param wetValue ADC value representing fully wet soil
     * \param reader Optional custom analogRead function (for unit tests)
     */
    explicit MoistureSensorHAL(uint16_t dryValue = 3724,
                               uint16_t wetValue = 0,
                               AnalogReader reader = nullptr);

    /*!
     * \brief Destructor
     */
    ~MoistureSensorHAL() = default;

    /*!
     * \brief Initialize the moisture sensor hardware
     * \return true if successful
     */
    bool begin();

    /*!
     * \brief Read soil moisture level as a percentage (0-100)
     * \return Moisture level percentage
     */
    uint8_t readMoistureLevel();

  private:
    /*!
     * \brief Read and average multiple analog samples
     * \param samples Number of samples
     * \return Averaged analog value
     */
    int readAveragedAnalog(uint8_t samples = 5);

    uint8_t m_moisture_pin; //!< Analog pin for soil moisture sensor
    uint16_t m_dryValue;    //!< ADC value for dry soil
    uint16_t m_wetValue;    //!< ADC value for wet soil
    AnalogReader m_reader;  //!< Function to read analog values
};

} // namespace Drivers
} // namespace PlantMonitor
