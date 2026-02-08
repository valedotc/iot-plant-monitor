#pragma once

/*!
 * \file iot-task-types.h
 * \brief Shared types, constants, and enumerations for the IoT task
 *
 * This header defines all common data structures used across the IoT task modules:
 * - Timing constants for FSM and communication
 * - State machine enumerations
 * - Configuration parameter indices
 * - Message structures for inter-task communication
 */

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "../sensor/sensor-task.h"
#include "utils/configuration/config.h"

// Forward declaration to avoid circular includes
namespace PlantMonitor {
namespace Drivers {
class WiFiHal;
}
} // namespace PlantMonitor

namespace PlantMonitor {
namespace Tasks {

// ============================================================================
// TIMING CONSTANTS
// ============================================================================

/*!
 * \defgroup IoTTimingConstants IoT Timing Constants
 * \brief Timing configuration for IoT task operations
 * @{
 */

constexpr uint32_t IOT_MQTT_PUB_INTERVAL_MS = 5000;  //!< Interval between MQTT publishes
constexpr uint32_t IOT_RECONNECT_DELAY_MS = 1000;    //!< Delay before retrying connection
constexpr uint32_t IOT_FSM_TICK_MS = 20;             //!< FSM tick interval
constexpr uint32_t IOT_WIFI_TIMEOUT_MS = 30000;      //!< WiFi connection timeout
constexpr uint32_t IOT_WIFI_TEST_TIMEOUT_MS = 15000; //!< WiFi test timeout during BLE config
constexpr uint32_t IOT_MAX_MQTT_INIT_RETRIES = 3;    //!< Maximum MQTT initialization retries
constexpr uint32_t IOT_MAX_CONFIG_LOAD_FAILS = 5;    //!< Max config load failures before reset

/*! @} */

// ============================================================================
// VERSION INFO
// ============================================================================

/*!
 * \defgroup IoTVersionInfo IoT Version Information
 * @{
 */

constexpr const char *IOT_FW_VERSION = "1.0.0"; //!< Firmware version
constexpr const char *IOT_HW_VERSION = "ESP32"; //!< Hardware version

/*! @} */

// ============================================================================
// PARAMETER INDICES
// ============================================================================

/*!
 * \enum ParamIndex
 * \brief Indices for configuration parameters array
 */
enum class ParamIndex : uint8_t {
    PlantTypeId = 0,   //!< Plant type identifier
    TempMin = 1,       //!< Minimum temperature threshold
    TempMax = 2,       //!< Maximum temperature threshold
    HumidityMin = 3,   //!< Minimum humidity threshold
    HumidityMax = 4,   //!< Maximum humidity threshold
    MoistureMin = 5,   //!< Minimum soil moisture threshold
    MoistureMax = 6,   //!< Maximum soil moisture threshold
    LightHoursMin = 7, //!< Minimum light hours required
    DeviceId = 8,      //!< Device identifier
};

// ============================================================================
// FSM STATE
// ============================================================================

/*!
 * \enum IoTState
 * \brief States of the IoT task finite state machine
 */
enum class IoTState {
    Boot,           //!< Initial boot state - check configuration
    BleAdvertising, //!< BLE advertising, waiting for connection
    BleConfiguring, //!< BLE connected, processing commands
    BleTestingWifi, //!< Testing WiFi credentials
    WifiConnecting, //!< Connecting to configured WiFi
    MqttOperating,  //!< Normal operation - publishing telemetry
    Error           //!< Error state - recovery pending
};

// ============================================================================
// BLE MESSAGE
// ============================================================================

/*!
 * \brief Maximum size for BLE message buffer
 */
constexpr size_t BLE_MESSAGE_MAX_SIZE = 512;

/*!
 * \struct BleMessage
 * \brief Message structure for BLE data reception queue
 */
struct BleMessage {
    char data[BLE_MESSAGE_MAX_SIZE]; //!< Message content buffer
    size_t length;                   //!< Actual message length
};

// ============================================================================
// SYSTEM CONTEXT
// ============================================================================

/*!
 * \struct IoTContext
 * \brief Context structure for the IoT task FSM
 *
 * Contains all state information needed across FSM iterations.
 */
struct IoTContext {
    IoTState currentState;       //!< Current FSM state
    int deviceId;                //!< Device identifier from config
    uint32_t lastMqttPublish;    //!< Timestamp of last MQTT publish
    uint32_t wifiConnectStart;   //!< Timestamp when WiFi connection started
    uint32_t mqttInitRetries;    //!< MQTT initialization retry counter
    uint32_t configLoadFailures; //!< Config load failure counter
    QueueHandle_t bleQueue;      //!< Queue for BLE messages

    // Pending configuration during WiFi test
    bool hasPendingConfig;   //!< True if config waiting for WiFi test
    AppConfig pendingConfig; //!< Temporary config during test
    String pendingCmd;       //!< Command that triggered WiFi test

    // WiFi test state
    uint32_t wifiTestStart;     //!< Timestamp when WiFi test started
    Drivers::WiFiHal *testWifi; //!< Temporary WiFi instance for testing
    int lastProgressSent;       //!< Last progress percentage sent via BLE
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/*!
 * \brief Converts an IoTState enum value to its string representation
 * \param state The state to convert
 * \return String representation of the state
 */
inline const char *iotStateToString(IoTState state) {
    switch (state) {
        case IoTState::Boot:
            return "BOOT";
        case IoTState::BleAdvertising:
            return "BLE_ADV";
        case IoTState::BleConfiguring:
            return "BLE_CFG";
        case IoTState::BleTestingWifi:
            return "BLE_TEST_WIFI";
        case IoTState::WifiConnecting:
            return "WIFI_CONN";
        case IoTState::MqttOperating:
            return "MQTT_OP";
        case IoTState::Error:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

/*!
 * \brief Gets a configuration parameter with a default fallback value
 * \param cfg The application configuration
 * \param index The parameter index
 * \param defaultVal Default value if index out of bounds
 * \return The parameter value or default
 */
inline float getConfigParam(const AppConfig &cfg, ParamIndex index, float defaultVal = 0.0f) {
    size_t idx = static_cast<size_t>(index);
    if (idx < cfg.params.size()) {
        return cfg.params[idx];
    }
    return defaultVal;
}

/*!
 * \brief Extracts the device ID from configuration
 * \param cfg The application configuration
 * \return Device ID, defaults to 1 if not configured
 */
inline int getDeviceIdFromConfig(const AppConfig &cfg) {
    return static_cast<int>(getConfigParam(cfg, ParamIndex::DeviceId, 1.0f));
}

} // namespace Tasks
} // namespace PlantMonitor
