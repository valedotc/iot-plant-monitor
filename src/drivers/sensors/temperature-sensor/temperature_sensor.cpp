#include "temperature_sensor.h"

namespace PlantMonitor {
namespace Drivers {

bme280HAL::bme280HAL() {
}

void bme280HAL::begin() {
    if (!bme.begin(0x76, &Wire)) {
        Serial.print("[ BME ] Error: not found!");
        while (1)
            ;
    }
}
/// @brief
/// @return
float bme280HAL::readTemperature() {
    return bme.readTemperature();
}

float bme280HAL::readHumidity() {
    return bme.readHumidity();
}

float bme280HAL::readAltitude() {
    return bme.readAltitude(SEALEVELPRESSURE_HPA);
}

float bme280HAL::readPressure() {
    return bme.readPressure() / 100.0F;
}
} // namespace Drivers
} // namespace PlantMonitor