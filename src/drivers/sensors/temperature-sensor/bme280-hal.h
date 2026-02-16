#pragma once

#include <Wire.h>
#include <Adafruit_BME280.h>
/*!
 * \file temperature_sensor.h
 * \brief bme280 sensor hardware abstraction layer (HAL)
 *
 * This class provides high level apis to read temperature, air humidity, atmospheric pressure.
 */
#define SEA_LEVEL_PRESSURE_HPA (1023.25f) //!< Default sea level pressure in hPa
#define BME280_I2C_ADDRESS (0x76u)        //!< Default I2C address for BME280

namespace PlantMonitor {
namespace Drivers {

/*!
 * \class Bme280Hal
 * \brief Read the air temperature, air humidity, atmospheric pressure and altidute, using bme280 sensor.
 */
class Bme280Hal {
  public:
    /*!
    * \brief Constructor
    */
    explicit Bme280Hal();

    /*!
     * \brief Destructor
     */
    ~Bme280Hal() = default;

    /*!
     * \brief Automatic initialization of the sensor
     */
    bool begin();

    /*!
     * \brief Take the bme280 air temperature measurement
     * \return Temperature in Celsius scale
     */
    float readTemperature();

    /*!
     * \brief Take the bme280 air pressure measurement
     * \return Pressure in Bar scale
     */
    float readPressure();

    /*!
     * \brief Take the bme280 Altitude estimate
     * \return Altitude in metres
     */
    float readAltitude();

    /*!
     * \brief Take the bme280 air Humidity measurement
     * \return Humidity percentage
     */
    float readHumidity();

  private:
    Adafruit_BME280 bme; //!< BME280 sensor instance
};

} // namespace Drivers
} // namespace PlantMonitor
