#include <Arduino.h>
#include "drivers/display/display_hal.h"
#include "drivers/sensors/temperature-sensor/bme280_hal.h"
#include "utils/configuration/config.h"
#include "drivers/bluetooth/bluetooth_hal.h"

#define VRX_PIN 34 // ADC1
#define VRY_PIN 35 // ADC1

static const char* DEVICE_NAME = "ESP32-PlantMonitor";

using namespace PlantMonitor::Drivers;

//----------------------
//  GLOBAL VARIABOLS
//----------------------

DisplayHAL display;
bme280HAL Thpa_sensor;


BleUartHal ble;

static String bleMsg;
static volatile bool bleMsgReady = false;

void setup() {

    Serial.begin(115200);
    Serial.println("\n=== Plant Monitor Starting ===");

    Thpa_sensor.begin();

    if (!display.begin()) {
      Serial.println("ERRORE: Impossibile inizializzare il display!");
      while (1) {
        delay(1000);
      }
    }
    // Setup screen
    display.clear();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setTextColor(COLOR_WHITE);
    display.printf("BOOT OK\n");
    display.printf("BLE starting...\n");

    display.update();
    
    if (!ble.begin(DEVICE_NAME)){
      display.printf("[BLE] Failed setup: \n[1] reboot\n[2] check begin function");
      while (1) {
        delay(1000);
      }
    }

    // define message handling
    ble.setRxHandler(
      [](const BleUartHal::Bytes& data)  
        {
          // Example: treat payload as text for now
          bleMsg = "";
          bleMsg.reserve(data.size());
          for (auto b : data) bleMsg += (char)b;
          bleMsgReady = true;
        }

    );

    if(!ConfigHandler::isConfigured()){
      Serial.println("\nMANNAGGIA A DIO INCUNFIGURATO\n");
    }

}

void loop() {  
  if (bleMsgReady) {
    bleMsgReady = false;

    display.clear();
    display.setCursor(0, 0);
    display.setTextColor(COLOR_WHITE);
    display.printf("%s\n", bleMsg.c_str());
    display.update();

    
  }
  
}


/*
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
    */