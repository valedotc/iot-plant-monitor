#include "moisture_sensor_hal.h"

namespace PlantMonitor {
namespace Drivers {

MoistureSensorHAL::MoistureSensorHAL(uint16_t dryValue,
                                     uint16_t wetValue,
                                     AnalogReader reader)
    : m_moisture_pin(Config::SOIL_MOISTURE_PIN),
      m_dryValue(dryValue),
      m_wetValue(wetValue),
      m_reader(reader ? reader : [](uint8_t pin) { return analogRead(pin); }) {
}

bool MoistureSensorHAL::begin() {
    pinMode(m_moisture_pin, INPUT);
#ifdef MOISTURE_DEBUG
    Serial.println("[MoistureSensorHAL] Capacitive moisture sensor initialized successfully");
#endif
    return true;
}

int MoistureSensorHAL::readAveragedAnalog(uint8_t samples) {
    long sum = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        sum += m_reader(m_moisture_pin);
        delay(10);
    }
    return static_cast<int>(sum / samples);
}

uint8_t MoistureSensorHAL::readMoistureLevel() {
    int analog_value = readAveragedAnalog();

    if (analog_value >= m_dryValue) {
        return 0; // Completely dry
    }

    uint8_t moisture_percentage = map(analog_value, m_dryValue, m_wetValue, 0, 100);
    moisture_percentage = constrain(moisture_percentage, 0, 100);

#ifdef MOISTURE_DEBUG
    Serial.printf("[MoistureSensorHAL] Analog Value: %d, Moisture Level: %d%%\n",
                  analog_value,
                  moisture_percentage);
#endif

    return moisture_percentage;
}

} // namespace Drivers
} // namespace PlantMonitor
