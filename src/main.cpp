#include <Arduino.h>
#include "drivers/display/display_hal.h"
#include "drivers/sensors/temperature-sensor/bme280_hal.h"
#include "utils/configuration/config.h"

#include <NimBLEDevice.h> // bluetooth lib

#define VRX_PIN 34 // ADC1
#define VRY_PIN 35 // ADC1

static NimBLECharacteristic* txChar;  // ESP32 -> phone (Notify)
static bool connected = false;

static const char* DEVICE_NAME = "ESP32-PlantMonitor";
static const NimBLEUUID SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static const NimBLEUUID RX_UUID     ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"); // Write from phone
static const NimBLEUUID TX_UUID     ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"); // Notify to phone


using namespace PlantMonitor::Drivers;

// Istanza globale del display
DisplayHAL display;
bme280HAL Thpa_sensor;

std::string debug = "ciao";

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) override {
    connected = true;
    Serial.println("BLE connected");
  }

  void onDisconnect(NimBLEServer* pServer) override {
    connected = false;
    Serial.println("BLE disconnected. Restarting advertising...");
    NimBLEDevice::startAdvertising();
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* ch) override {
    std::string v = ch->getValue();
    if (v.empty()) return;

    Serial.print("RX from phone: ");
    Serial.println(v.c_str());

    if (connected && txChar) {
      std::string echo = "ESP32 dice: " + v;
      txChar->setValue(echo);
      txChar->notify();
    }
  }
};

void startBleUart() {
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9); // max TX power
  NimBLEDevice::setSecurityAuth(false, false, false); // no bonding for first test

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = server->createService(SERVICE_UUID);

  // TX notify characteristic (ESP32 -> phone)
  txChar = svc->createCharacteristic(TX_UUID, NIMBLE_PROPERTY::NOTIFY);

  // RX write characteristic (phone -> ESP32)
  NimBLECharacteristic* rxChar =
      svc->createCharacteristic(RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(new RxCallbacks());

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);

  // Optional: better compatibility with some phones
  adv->setMinInterval(32); // 20ms (32 * 0.625ms)
  adv->setMaxInterval(64); // 40ms

  adv->start();
  Serial.println("BLE advertising started");
}

void setup() {

    Serial.begin(115200);

    if(!ConfigHandler::isConfigured()){
      Serial.println("\nMANNAGGIA A DIO INCUNFIGURATO\n");
    }

    Thpa_sensor.begin();

    Serial.println("\n=== Plant Monitor Starting ===");

    if (!display.begin()) {
        Serial.println("ERRORE: Impossibile inizializzare il display!");
        while (1) {
            delay(1000);
        }
    }

    //Serial.println("\nCLEANING CONF\n");
    //ConfigHandler::clear();
    
    AppConfig cfg;
    
    if (ConfigHandler::load(cfg)) {
      Serial.println("Loaded config:");
    } else {
      Serial.println("No valid config, writing defaults...");
      cfg.ssid = "MyWiFi";
      cfg.password = "secret";
      cfg.params = {1.1f, 2.2f, 3.3f}; // any length you want
      ConfigHandler::save(cfg);
    }
    
    Serial.printf("SSID: %s\n", cfg.ssid.c_str());
    Serial.printf("PASS: %s\n", cfg.password.c_str());
    Serial.printf("Params count: %u\n", (unsigned)cfg.params.size());
    for (size_t i = 0; i < cfg.params.size(); i++) {
      Serial.printf("  p[%u]=%.4f\n", (unsigned)i, cfg.params[i]);
    }
    
    

    Serial.println("\nStarting Bluetooth...");
    startBleUart();

    // Setup screen
    display.clear();
    display.setTextSize(1);
    display.setTextColor(COLOR_WHITE);
}

void loop() {
    display.clear();
    display.setCursor(0, 0);
    
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
    static uint32_t last = 0;
    if (connected && millis() - last > 2000) {
        last = millis();
        char msg[64];
        snprintf(msg, sizeof(msg), "tick %lu\n", (unsigned long)(millis() / 1000));
        txChar->setValue((uint8_t*)msg, strlen(msg));
        txChar->notify();
        display.printf("\nTX to phone: \n");
        display.printf("%s" , msg);
        //display.printf(debug.c_str());
        display.update();
    }
    //delay(20);
}