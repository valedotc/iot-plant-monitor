#include <Arduino.h>
#include <iostream>
#include "drivers/display/display_hal.h"
#include "drivers/sensors/temperature-sensor/bme280_hal.h"
#include "utils/configuration/config.h"
#include "drivers/bluetooth/bluetooth_hal.h"
#include "drivers/wifi/wifi_hal.h"

#define VRX_PIN 34 // ADC1
#define VRY_PIN 35 // ADC1

static const char* DEVICE_NAME = "ESP32-PlantMonitor";


// ["fabio","arianna06",15.0,30.0,45.0,75.0,25.0,85.0]

using namespace PlantMonitor::Drivers;

//----------------------
//  GLOBAL VARIABOLS
//----------------------

DisplayHAL* display = nullptr;
bme280HAL* Thpa_sensor = nullptr;
BleUartHal* ble = nullptr;
WiFiHal* wifi = nullptr;

static String bleMsg;
static volatile bool bleMsgReady = false;

typedef enum state_t{
    BOOT,
    BLE_ADV,
    CONFIGURATION,
    WIFI,
    MQTT
} State;

State current_state;

void setup() {
    current_state = BOOT;

    Serial.begin(115200);
    Serial.println("\n=== Plant Monitor Starting ===");
  
    display = new DisplayHAL();
    Thpa_sensor = new bme280HAL();
    ble = new BleUartHal();

    Thpa_sensor->begin();

    if (!display->begin()) {
      Serial.println("ERRORE: Impossibile inizializzare il display!");
      while (1) {
        delay(1000);
      }
    }
    
    // Setup screen
    display->setTextSize(1);
    display->setCursor(0, 0);
    display->setTextColor(COLOR_WHITE);
    //display->printf("%d" , ConfigHandler::isConfigured());
    //display->update();
    
    if (!ble->begin(DEVICE_NAME)){
      display->printf("[BLE] Failed setup: \n[1] reboot\n[2] check begin function");
      while (1) {
        delay(1000);
      }
    }
    // define message handling
    ble->setRxHandler(
        [](const BleUartHal::Bytes& data)  
            {
                // Example: treat payload as text for now
                bleMsg = "";
                bleMsg.reserve(data.size());
                for (auto b : data) bleMsg += (char)b;
                bleMsgReady = true;
            }
    );
    
}

static uint32_t lastUi = 0;
void loop() {
    display->clear();
    display->setCursor(0,0);
    AppConfig cfg;

    switch (current_state){

        case BOOT:
            //Serial.println("\nBOOT\n");
            if(!ConfigHandler::isConfigured()){
                display->printf("\nMANNAGGIA A DIO INCUNFIGURATO\n");
                ble->startAdvertising_();
                display->printf("[BLE] advertising started...\nWaiting for connection...");
                display->update();
                current_state = BLE_ADV;
            } else{
                display->printf("Found Configuration\nStarting WiFi connection...");
                display->update();
                current_state = WIFI;
            }

            break;
        
        case BLE_ADV:
            //Serial.println("\nBLE_ADV\n");
            if (millis() - lastUi > 500) {
                display->clear();
                display->setCursor(0, 0);
                display->printf("[BLE] advertising started...\nWaiting for connection...");
                display->update();
                lastUi = millis();
            }

            if (ble->isConnected()) {
                current_state = CONFIGURATION;
                break;
            }

            break;
        
        case CONFIGURATION:
            if(!ble->isConnected()){
                current_state = BLE_ADV;
                break;
            }
            //Serial.println("\nCONFIGURATION\n");
            if (millis() - lastUi > 500) {
                display->clear();
                display->setCursor(0, 0);
                display->printf("[BLE] CONNECTED!\nWaiting for wifi and password...");
                display->update();
                lastUi = millis();
            }


            if (bleMsgReady){
                // configurazione arrivata, da parsare e validare... SE valida allora current_state = WIFI
                
                if (!ConfigHandler::parseAppCfg(bleMsg.c_str(), cfg)) {
                    display->printf("PARSER ERROR CHECK THE APP\n");
                    display->printf("\nGOING BACK TO BLE_ADV\n");
                    display->update();
                    current_state = BLE_ADV;
                    bleMsgReady = false;
                    break;
                }
                
                bleMsgReady = false;
                
                //Save on flash memory
                ConfigHandler::save(cfg);
                current_state = WIFI;
                
            }
            break;
        

        case WIFI:
            if (!ConfigHandler::isConfigured()){
                Serial.println("CHE CAZZO CI FAI QUI");
                current_state = BLE_ADV;
                break;
            }

            ConfigHandler::load(cfg);

            wifi = new WiFiHal(cfg.ssid.c_str() , cfg.password.c_str());
            if(!wifi->isConnected()){
                wifi->begin();
            } else{
                display->printf("\n\n CIAO WIFI\n");
                display->update();
            }
            
            break;
        default:
            display->clear();
            display->printf("\n\nDIOCANE STATO SCONOSCIUTO\n\n");
            display->update();
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