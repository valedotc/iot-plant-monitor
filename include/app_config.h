#pragma once

#include <Arduino.h>

/*!
 * \file app_config.h
 * \brief System-wide configuration for IoT Plant Monitor
 */

namespace Config {

// ============ I2C Configuration ============
constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;
constexpr uint32_t I2C_FREQUENCY = 400000; // 400kHz

// ============ Display Configuration ============
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 128;
constexpr uint8_t DISPLAY_I2C_ADDR = 0x3C;
constexpr int8_t DISPLAY_RESET_PIN = -1;

// ============ Display Colors (SSD1327 Grayscale 0-15) ============
constexpr uint8_t COLOR_BLACK = 0;
constexpr uint8_t COLOR_WHITE = 15;
constexpr uint8_t COLOR_GRAY_DARK = 5;
constexpr uint8_t COLOR_GRAY_MID = 8;
constexpr uint8_t COLOR_GRAY_LIGHT = 12;

// ============ Sensor Pins ============
constexpr uint8_t SOIL_MOISTURE_PIN = 34; // ADC1
constexpr uint8_t ONEWIRE_PIN = 25;       // DS18B20 //ToDel

// ============ Actuator Pins ============
constexpr uint8_t GROW_LIGHT_PIN = 35;

// ============ RGB LED Configuration ============
constexpr uint8_t RGB_LED_R_PIN = 6; // PWM //ToDel
constexpr uint8_t RGB_LED_G_PIN = 5; // PWM //ToDel
constexpr uint8_t RGB_LED_B_PIN = 3; // PWM //ToDel

// ============ FreeRTOS Configuration ============
namespace Tasks {
constexpr uint16_t SENSOR_STACK_SIZE = 4096;
constexpr uint8_t SENSOR_PRIORITY = 3;
constexpr uint8_t SENSOR_CORE = 0;

constexpr uint16_t DISPLAY_STACK_SIZE = 4096;
constexpr uint8_t DISPLAY_PRIORITY = 2;
constexpr uint8_t DISPLAY_CORE = 1;

constexpr uint16_t CONTROL_STACK_SIZE = 3072;
constexpr uint8_t CONTROL_PRIORITY = 3;
constexpr uint8_t CONTROL_CORE = 0;

constexpr uint16_t IOT_STACK_SIZE = 8192;
constexpr uint8_t IOT_PRIORITY = 1;
constexpr uint8_t IOT_CORE = 0;
} // namespace Tasks

// ============ Timing Configuration ============
namespace Timing {
constexpr uint32_t SENSOR_READ_INTERVAL_MS = 10000;      // 10s
constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 100;     // 10 FPS
constexpr uint32_t BIOELECTRIC_SAMPLE_INTERVAL_MS = 500; // 2 Hz
constexpr uint32_t IOT_PUBLISH_INTERVAL_MS = 60000;      // 1 min
} // namespace Timing

// ============ WiFi Configuration ============
namespace WiFi {
constexpr const char *SSID = "SSID";
constexpr const char *PASSWORD = "PASSWORD";
} // namespace WiFi

// ============ MQTT Configuration ============
namespace MQTT {
constexpr const char *BROKER = "broker.hivemq.com";
constexpr uint16_t PORT = 1883;
constexpr const char *CLIENT_ID = "PlantMonitor";
constexpr const char *TOPIC_STATUS = "plant/status";
constexpr const char *TOPIC_SENSORS = "plant/sensors";
} // namespace MQTT

} // namespace Config