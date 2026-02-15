#include <Wire.h>
#include <Arduino.h>
#include "app-config.h"

namespace PlantMonitor{
namespace Drivers{

class LightSensor{

    public:

        explicit LightSensor(uint8_t pin = Config::GROW_LIGHT_PIN);

        void begin();
        int readRaw();
        int readRawAverage(uint8_t samples = 10);  // Read multiple samples and return average
        float readVoltage(float vref = 3.3f) ;
        float readPercentage(int minRaw = 0, int maxRaw = 4095) ; // Maps raw to 0..100%

    private:
        uint8_t _pin;
};

} //namespace Drivers
}//namespace PlantMonitor