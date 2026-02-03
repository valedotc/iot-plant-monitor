#include "button_sensor.h"

namespace PlantMonitor {
namespace Drivers {



bool ButtonHal::debouncing(){  //funzione per debouncing switch
  if(millis()-lastIntTime > debounceDelay){
    lastIntTime = millis();
    Serial.println("true");
    return true;
  } else {
    return false;
    Serial.println("false");
  }
}

} // namespace Drivers
} // namespace PlantMonitor