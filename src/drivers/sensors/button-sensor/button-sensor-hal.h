#pragma once

#include <Arduino.h>

#define DEBOUNCE_DELAY 200

namespace PlantMonitor {
namespace Drivers {

enum ButtonInputMode {
    BUTTON_INPUT_PULLUP = INPUT_PULLUP,
    BUTTON_INPUT_PULLDOWN = INPUT_PULLDOWN

};

class ButtonHal{
    public:
        explicit ButtonHal(uint8_t pin, enum ButtonInputMode mode = BUTTON_INPUT_PULLUP, void (*handler)() = nullptr);

        bool debouncing();

        uint8_t getPin() const;

    private:
        uint8_t pin;
        void (*interruptHandler)();
        unsigned long int lastIntTime = 0;
};

} // namespace Drivers
} // namespace PlantMonitor