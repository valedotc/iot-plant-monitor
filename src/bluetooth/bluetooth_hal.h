#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

//  notify esp32 -> phone
static NimBLECharacteristic* txChar;  
static bool connected = false;

static const char* DEVICE_NAME = "ESP32-PlantMonitor";
static const NimBLEUUID SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static const NimBLEUUID RX_UUID     ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E"); // Write from phone
static const NimBLEUUID TX_UUID     ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E"); // Notify to phone

