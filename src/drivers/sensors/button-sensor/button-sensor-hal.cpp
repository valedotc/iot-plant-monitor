#include "button-sensor-hal.h"

namespace PlantMonitor {
namespace Drivers {

ButtonHal::ButtonHal(uint8_t pin, enum ButtonInputMode mode, void (*interruptHandler)()) : pin(pin) {
    pinMode(pin, mode);
    if(interruptHandler != nullptr){
        this->interruptHandler = interruptHandler;
        attachInterrupt(pin, interruptHandler, FALLING);
    }
}

bool ButtonHal::debouncing(){ 
  if(millis()-lastIntTime > DEBOUNCE_DELAY){
    lastIntTime = millis();
    return true;
  } else {
    return false;
  }
}

uint8_t ButtonHal::getPin() const {
    return pin;
}

} // namespace Drivers
} // namespace PlantMonitor