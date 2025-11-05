#include <Arduino.h>
#include "drivers/display/display_hal.h"

#define VRX_PIN 34   // ADC1
#define VRY_PIN 35   // ADC1

using namespace PlantMonitor::Drivers;

// Istanza globale del display
DisplayHAL display;

// Bitmap monocromatica invertita (0 = bianco, 1 = nero per display OLED)

void setup() {

    Serial.begin(115200);
    Serial.println("\n=== Plant Monitor Starting ===");
    
    if (!display.begin()) {
        Serial.println("ERRORE: Impossibile inizializzare il display!");
        while(1) {
            delay(1000);
        }
    }
    
    // Test testo
    display.clear();
    display.setTextSize(1);
    display.setTextColor(COLOR_WHITE);

    // Disegna bitmap (centrata a 0,0 su display 128x128)
    display.clear();
    display.setCursor(30, 110);
}

void loop() {
    display.clear();
    display.setCursor(30, 64);
    
    int xValue = analogRead(VRX_PIN);
    int yValue = analogRead(VRY_PIN);
    
    int screenX = round((xValue * display.getWidth()) / 4096);
    int screenY = round((yValue * display.getWidth()) / 4096);
    
    display.drawCircle(screenX , screenY , 3 , COLOR_WHITE);
    //display.printf("X = %d" , screenX);
    //display.drawPixel(screenX , screenY , COLOR_WHITE);
    display.update();
    
    //delay(20);
}