#include "light-sensor.h"

namespace PlantMonitor{
namespace Drivers{


LightSensor::LightSensor(uint8_t pin): _pin(pin){}

void LightSensor::begin(){
    pinMode(_pin , INPUT);
}

int LightSensor::readRaw(){
    return analogRead(_pin);
}

float LightSensor::readVoltage(float vref) {
    int raw = analogRead(_pin);
    return  raw / 4095.0f;
}

float  LightSensor::readPercentage(int minRaw, int maxRaw) {
    int raw = analogRead(_pin);

    // Clamp
    if (raw < minRaw) raw = minRaw;
    if (raw > maxRaw) raw = maxRaw;

    const int span = (maxRaw - minRaw);
    if (span <= 0) return 0.0f;

    return ((raw - minRaw) / span) * 100.0f;
}


} // namespace Drivers
} // namespace PlantMonitor