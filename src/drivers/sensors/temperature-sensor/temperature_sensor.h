#pragma once

#include <Wire.h>
#include <Adafruit_BME280.h>
#include "app_config.h"

#define SEALEVELPRESSURE_HPA (1023.25)

namespace PlantMonitor{
namespace Drivers{

class bme280HAL{
    public:
        
        explicit bme280HAL();
        
        void begin();

        float readTemperature();
        float readPressure();
        float readAltitude();
        float readHumidity();
    
    private:
        Adafruit_BME280 bme;

};


}
}

