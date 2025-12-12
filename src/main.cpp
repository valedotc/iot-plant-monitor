#include <Arduino.h>
#include "drivers/display/display_hal.h"
#include "drivers/sensors/temperature-sensor/temperature_sensor.h"

#define VRX_PIN 34 // ADC1
#define VRY_PIN 35 // ADC1

using namespace PlantMonitor::Drivers;

// Istanza globale del display
DisplayHAL display;
bme280HAL Thpa_sensor;

// Bitmap monocromatica invertita (0 = bianco, 1 = nero per display OLED)

void setup() {

    Serial.begin(115200);
    Thpa_sensor.begin();

    Serial.println("\n=== Plant Monitor Starting ===");

    if (!display.begin()) {
        Serial.println("ERRORE: Impossibile inizializzare il display!");
        while (1) {
            delay(1000);
        }
    }

    // Setup screen
    display.clear();
    display.setTextSize(1);
    display.setTextColor(COLOR_WHITE);
}

void loop() {
    display.clear();
    display.setCursor(0, 0);

    display.print("Temperature = ");
    display.printf("%f", Thpa_sensor.readTemperature());
    display.printf(" *C\n");

    display.print("Pressure = ");

    display.printf("%f", Thpa_sensor.readPressure());
    display.printf(" hPa\n");

    display.print("Approx. Altitude = ");
    display.printf("%f", Thpa_sensor.readAltitude());
    display.printf(" m\n");

    display.print("Humidity = ");
    display.printf("%f", Thpa_sensor.readHumidity());
    display.printf(" %\n");

    //display.drawPixel(screenX , screenY , COLOR_WHITE);
    display.update();

    //delay(20);
}