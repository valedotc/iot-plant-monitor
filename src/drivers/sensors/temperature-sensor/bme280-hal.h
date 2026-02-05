#pragma once

#include <Wire.h>
#include <Adafruit_BME280.h>
#include "app-config.h"
/*!
 * \file temperature_sensor.h
 * \brief bme280 sensor hardware abstraction layer (HAL)
 *
 * This class provides high level apis to read temperature, air humidity, atmospheric pressure.
 */
#define SEALEVELPRESSURE_HPA (1023.25)

namespace PlantMonitor {
namespace Drivers {

/*!
 * \class Bme280Hal
 *
 * \brief Read the air temperature, air humidity, atmospheric pressure and altidute, using bme280 sensor.
 */
class Bme280Hal {
  public:
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
    Adafruit_BME280 bme;
};

} // namespace Drivers
} // namespace PlantMonitor
