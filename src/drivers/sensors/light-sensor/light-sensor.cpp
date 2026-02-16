#include "light-sensor.h"

namespace PlantMonitor {
namespace Drivers {

LightSensor::LightSensor(uint8_t pin)
    : _pin(pin) {
}

void LightSensor::begin() {
    pinMode(_pin, INPUT);
}

int LightSensor::readRaw() {
    return analogRead(_pin);
}

int LightSensor::readRawAverage(uint8_t samples) {
    if (samples == 0) samples = 1;

    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += analogRead(_pin);
        if (i < samples - 1) {
            delayMicroseconds(100);  // Small delay between readings
        }
    }

    return sum / samples;
}

float LightSensor::readVoltage(float vref) {
    int raw = analogRead(_pin);
    return raw / 4095.0f;
}

float LightSensor::readPercentage(int minRaw, int maxRaw) {
    int raw = analogRead(_pin);

    // Clamp
    if (raw < minRaw)
        raw = minRaw;
    if (raw > maxRaw)
        raw = maxRaw;

    const int span = (maxRaw - minRaw);
    if (span <= 0)
        return 0.0f;

    return ((raw - minRaw) / span) * 100.0f;
}

} // namespace Drivers
} // namespace PlantMonitor