#pragma once

#include <Arduino.h>

#define SWITCH_PIN 24

namespace PlantMonitor {
namespace Drivers {

class ButtonHal{
    public:
        explicit ButtonHal() = default;

        bool debouncing();

    private:
        const int SwitchPin = SWITCH_PIN;

        const int debounceDelay = 400; //roba per il debouncing
        long lastIntTime = 0;
};

} // namespace Drivers
} // namespace PlantMonitor