#include <Wire.h>
#include <Arduino.h>
#include "app-config.h"

/*!
 * \file light-sensor.h
 * \brief Light Sensor Driver
 */

#define LIGHT_SENSOR_DEFAULT_PIN Config::GROW_LIGHT_PIN //!< Default GPIO pin for light sensor
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

        void begin();
        int readRaw();
        int readRawAverage(uint8_t samples = 10);  // Read multiple samples and return average
        float readVoltage(float vref = 3.3f) ;
        float readPercentage(int minRaw = 0, int maxRaw = 4095) ; // Maps raw to 0..100%

    /*!
         * \brief Initialize the light sensor
         * \return true if initialization was successful, false otherwise
         */
    void begin();

    /*!
         * \brief Read the raw analog value from the light sensor
         * \return Raw analog value
         */
    int readRaw();

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
    float readPercentage(int minRaw = ADC_MIN_VALUE, int maxRaw = ADC_MAX_VALUE); // Maps raw to 0..100%

  private:
    uint8_t _pin; //!< GPIO pin number
};

} //namespace Drivers
} //namespace PlantMonitor