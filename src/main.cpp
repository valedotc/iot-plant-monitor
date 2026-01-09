#include <Arduino.h>
#include <iostream>
#include <WiFiClientSecure.h>
#include "drivers/display/display_hal.h"
#include "drivers/sensors/temperature-sensor/bme280_hal.h"
#include "utils/configuration/config.h"
#include "drivers/bluetooth/bluetooth_hal.h"
#include "drivers/wifi/wifi_hal.h"
#include "drivers/sensors/moisture-sensor/moisture_sensor_hal.h"
#include "iot/mqtt_service.h"
#include "iot/hivemq_ca.h"
#include "utils/configuration/private_data.h"



#define VRX_PIN 34 // ADC1
#define VRY_PIN 35 // ADC1

static const char* DEVICE_NAME = "ESP32-PlantMonitor";
static const char* ESP_ID      = "esp32_001"; //per ora lo metto fisso ma va implementato



PlantMonitor::Drivers::MoistureSensorHAL* moisture = nullptr;

static WiFiClientSecure* tlsClient = nullptr;
static PlantMonitor::IoT::MqttService* mqtt = nullptr;

//creo il topic telemetry con l'uso dell'id dell'esp
static String makeTelemetryTopic() {
  String t = "plantform/";
  t += ESP_ID;
  t += "/telemetry";
  return t;
}

using namespace PlantMonitor::Drivers;

//----------------------
//  GLOBAL VARIABOLS
//----------------------

DisplayHAL* display = nullptr;
bme280HAL* Thpa_sensor = nullptr;
MoistureSensorHAL* umidity = nullptr;
BleUartHal* ble = nullptr;
WiFiHal* wifi = nullptr;

static String bleMsg;
static volatile bool bleMsgReady = false;


// pubblico la telemetry ogni N secondi
            static uint32_t lastPub = 0;
            const uint32_t PUB_MS = 20000;

typedef enum state_t{
    BOOT,
    BLE_ADV,
    CONFIGURATION,
    WIFI,
    MQTT
} State;

State current_state;

static void makeFakeTelemetry(float& temperature, float& humidity, uint8_t& chlorophyll) {
    temperature = 45;             
    humidity    = 75;      
    chlorophyll = 59; 
    Serial.println(temperature);
    Serial.println(humidity);
    Serial.println(chlorophyll);
}

static String buildTelemetryJson(float temperature, float humidity, uint8_t chlorophyll) {
  if (temperature < 0) temperature = 0; if (temperature > 100) temperature = 100;
  if (humidity < 0) humidity = 0; if (humidity > 100) humidity = 100;
  if (chlorophyll > 100) chlorophyll = 100;

  char buf[160];

  snprintf(buf, sizeof(buf),
           "{\n"
           "  \"temperature\": %.2f,\n"
           "  \"humidity\": %.2f,\n"
           "  \"chlorophyll\": %u\n"
           "}",
           temperature, humidity, (unsigned)chlorophyll);

  Serial.print(buf);
  return String(buf);
}


static bool mqttConnectOrRetry() {
  if (!mqtt) return false;
  if (mqtt->isConnected()) return true;

  Serial.println("[MQTT] Connecting...");
  bool ok = mqtt->begin();
  Serial.printf("[MQTT] begin() => %s\n", ok ? "OK" : "FAIL");
  return ok;
}

void setup() {
    current_state = BOOT;

    Serial.begin(115200);
    Serial.println("\n=== Plant Monitor Starting ===");
  
    display = new DisplayHAL();
    Thpa_sensor = new bme280HAL();
    ble = new BleUartHal();

    Thpa_sensor->begin();
    moisture = new MoistureSensorHAL();
    moisture->begin();

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
                bleMsg = "";
                bleMsg.reserve(data.size());
                for (auto b : data) bleMsg += (char)b;
                bleMsgReady = true;
            }
    );

    ConfigHandler::clear();
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
                display->printf("\nINCUNFIGURATO\n");
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
                Serial.println("RETURNING TO BLE");
                current_state = BLE_ADV;
                break;
            }

            ConfigHandler::load(cfg);

            wifi = new WiFiHal(cfg.ssid.c_str() , cfg.password.c_str());
            Serial.printf("%s | %s" , cfg.ssid.c_str() , cfg.password.c_str() );
            if(!wifi->isConnected()){
                wifi->begin();
            } 
            if (wifi->isConnected()) {
                Serial.println("[WIFI] Connected OK => MQTT");
                display->clear();
                display->setCursor(0, 0);
                display->printf("WIFI CONNECTED\n");
                display->update();

                current_state = MQTT;
            }
            
            break;

        case MQTT:
            if (!tlsClient) {
                tlsClient = new WiFiClientSecure();
                tlsClient->setCACert(HIVEMQ_ROOT_CA);

                Serial.println("[TLS] Probing raw TLS connection to broker...");
                if (tlsClient->connect(MQTT_BROKER, MQTT_PORT)) {
                    Serial.println("[TLS] TLS connect OK");
                    tlsClient->stop();
                } else {
                    Serial.println("[TLS] TLS connect FAILED");
                }
                mqtt = new PlantMonitor::IoT::MqttService(tlsClient, MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD);

                mqtt->setMessageCallback([](String topic, String payload) {
                Serial.print("[MQTT] RX topic: ");
                Serial.println(topic);
                Serial.print("[MQTT] RX payload: ");
                Serial.println(payload);
                });

                Serial.println("[MQTT] Service created");
            }

            // Connect if needed
            if (!mqttConnectOrRetry()) {
                display->clear();
                display->setCursor(0, 0);
                display->printf("MQTT CONNECT FAIL\n");
                display->printf("Retry...\n");
                display->update();
                delay(1000);
                break;
            }

            mqtt->poll();

            if (millis() - lastPub >= PUB_MS) {
                lastPub = millis();

                float temperature = 0, 
                humidity = 0;
                uint8_t chlorophyll = 0;

                const bool USE_FAKE = true; // per switch tra fake sensori e real

                if (USE_FAKE) {
                    makeFakeTelemetry(temperature, humidity, chlorophyll);
                    Serial.println("[TEL] Using FAKE telemetry");
                } else {
                    temperature = Thpa_sensor->readTemperature();
                    humidity    = Thpa_sensor->readHumidity();
                    // uso moisture come “chlorophyll” fino a quando non abbiamo un altro sensore
                    chlorophyll = moisture->readMoistureLevel();
                    Serial.println("[TEL] Using REAL telemetry (temp/hum + moisture as chlorophyll)");
                }

                String payload = buildTelemetryJson(temperature, humidity, chlorophyll);
                String topic = makeTelemetryTopic();

                Serial.print("[MQTT] Publishing to ");
                Serial.println(topic);
                Serial.println(payload);

                bool ok = mqtt->publish(topic.c_str(), payload.c_str(), false);
                Serial.printf("[MQTT] publish() => %s\n", ok ? "OK" : "FAIL");

                // UI
                display->clear();
                display->setCursor(0, 0);
                display->printf("MQTT: %s\n", mqtt->isConnected() ? "OK" : "NO");
                display->printf("T: %.2f\nH: %.2f\nC: %u\n", temperature, humidity, chlorophyll);
                display->update();
            }

            break;

        default:
            display->clear();
            display->printf("\n\nSTATO SCONOSCIUTO\n\n");
            display->update();
    }

    delay(10);
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