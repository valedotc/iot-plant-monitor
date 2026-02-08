#include "bme280-hal.h"

namespace PlantMonitor {
namespace Drivers {

Bme280Hal::Bme280Hal() {
}

bool Bme280Hal::begin() {
    if (!bme.begin(BME280_I2C_ADDRESS, &Wire)) {
        Serial.print("[ BME ] Error: not found!");
        return false;
    }
    return true;
}

float Bme280Hal::readTemperature() {
    return bme.readTemperature();
}

float Bme280Hal::readHumidity() {
    return bme.readHumidity();
}

float Bme280Hal::readAltitude() {
    return bme.readAltitude(SEA_LEVEL_PRESSURE_HPA);
}

float Bme280Hal::readPressure() {
    return bme.readPressure() / 100.0f; // Convert to hPa
}
} // namespace Drivers
} // namespace PlantMonitor