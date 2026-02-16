#pragma once

#include <Arduino.h>

/*!
 * \file app-config.h
 * \brief System-wide hardware and task configuration for IoT Plant Monitor.
 */

namespace Config {

// ============ I2C Bus ============
constexpr uint32_t I2C_FREQUENCY = 400000; //!< I2C clock speed (400 kHz)

// ============ Display (SH1107 OLED) ============
constexpr uint8_t DISPLAY_WIDTH = 128;
constexpr uint8_t DISPLAY_HEIGHT = 128;
constexpr uint8_t DISPLAY_I2C_ADDR = 0x3C;
constexpr int8_t DISPLAY_RESET_PIN = -1; //!< No hardware reset pin

// ============ Sensor Pins ============
constexpr uint8_t SOIL_MOISTURE_PIN = 34; //!< Capacitive moisture sensor (ADC1)
constexpr uint8_t LIGHT_SENSOR_PIN = 35;  //!< Photoresistor (ADC1)
constexpr uint8_t BUTTON_PIN = 32;        //!< Tactile button (internal pull-up)

// ============ FreeRTOS Tasks ============
namespace Tasks {

constexpr uint16_t DISPLAY_STACK_SIZE = 4096;
constexpr UBaseType_t DISPLAY_PRIORITY = 3; //!< Highest - UI must stay responsive
constexpr BaseType_t DISPLAY_CORE = 0;

constexpr uint16_t SENSOR_STACK_SIZE = 4096;
constexpr UBaseType_t SENSOR_PRIORITY = 2;
constexpr BaseType_t SENSOR_CORE = 1;

constexpr uint16_t IOT_STACK_SIZE = 8192; //!< Larger stack for TLS + JSON
constexpr UBaseType_t IOT_PRIORITY = 1;   //!< Lowest - networking is best-effort
constexpr BaseType_t IOT_CORE = 1;        //!< Separate from display core

} // namespace Tasks

} // namespace Config
