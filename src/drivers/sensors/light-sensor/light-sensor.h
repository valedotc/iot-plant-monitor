#pragma once

#include <Arduino.h>
#include "app-config.h"

/*!
 * \file light-sensor.h
 * \brief Light Sensor Driver
 */

#define LIGHT_SENSOR_DEFAULT_PIN Config::LIGHT_SENSOR_PIN //!< Default GPIO pin for light sensor
#define ADC_MAX_VALUE (4095u)                           //!< Maximum ADC value for 12-bit resolution
#define ADC_MIN_VALUE (0u)                              //!< Minimum ADC value
#define ADC_REF_VOLTAGE (3.3f)                          //!< Reference voltage for ADC

namespace PlantMonitor {
namespace Drivers {

/*!
 * \class LightSensor
 * \brief Driver for a light sensor connected to an analog pin
 */
class LightSensor {
  public:
    LightSensor(uint8_t pin = LIGHT_SENSOR_DEFAULT_PIN);

    /*!
     * \brief Initialize the light sensor
     */
    void begin();

    /*!
     * \brief Read the raw analog value from the light sensor
     * \return Raw analog value
     */
    int readRaw();

    /*!
     * \brief Read multiple samples and return average
     * \param samples Number of samples to average (default: 10)
     * \return Average raw analog value
     */
    int readRawAverage(uint8_t samples = 10);

    /*!
     * \brief Read the voltage from the light sensor
     * \param vref Reference voltage for ADC conversion (default: 3.3V)
     * \return Voltage value
     */
    float readVoltage(float vref = ADC_REF_VOLTAGE);

    /*!
     * \brief Read the light intensity as a percentage (0-100%)
     * \param minRaw Minimum raw value corresponding to 0% (default: 0)
     * \param maxRaw Maximum raw value corresponding to 100% (default: 4095)
     * \return Light intensity percentage
     */
    float readPercentage(int minRaw = ADC_MIN_VALUE, int maxRaw = ADC_MAX_VALUE);

  private:
    uint8_t _pin; //!< GPIO pin number
};

} // namespace Drivers
} // namespace PlantMonitor
