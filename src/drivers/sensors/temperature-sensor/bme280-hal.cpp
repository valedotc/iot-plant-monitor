#include "bme280-hal.h"

namespace PlantMonitor {
namespace Drivers {

Bme280Hal::Bme280Hal() {
}

bool Bme280Hal::begin() {
    if (!bme.begin(0x76, &Wire)) {
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
    return bme.readAltitude(SEALEVELPRESSURE_HPA);
}

float Bme280Hal::readPressure() {
    return bme.readPressure() / 100.0F;
}
} // namespace Drivers
} // namespace PlantMonitor