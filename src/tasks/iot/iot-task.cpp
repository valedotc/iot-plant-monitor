/*!
 * \file iot-task.cpp
 * \brief IoT Task FSM Orchestrator
 *
 * Main finite state machine for IoT operations.
 * Coordinates BLE configuration, WiFi connection, and MQTT telemetry.
 */

#include <esp_task_wdt.h>
#include <time.h>
#include "iot-task.h"
#include "iot-task-types.h"
#include "ble-protocol.h"
#include "mqtt-telemetry.h"
#include "drivers/bluetooth/bluetooth-hal.h"
#include "drivers/wifi/wifi-hal.h"
#include "iot/hivemq-ca.h"
#include "utils/configuration/private-data.h"

using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

// ============================================================================
// STATIC STATE
// ============================================================================

/*!
 * \defgroup IoTStaticState IoT Task Static State
 * @{
 */

static IoTContext s_ctx;                            //!< FSM context
static BleUartHal *s_ble = nullptr;                 //!< BLE UART controller
static BleProtocolHandler *s_bleProtocol = nullptr; //!< BLE protocol handler
static WiFiHal *s_wifi = nullptr;                   //!< WiFi manager
static MqttTelemetryPublisher *s_mqtt = nullptr;    //!< MQTT telemetry publisher

/*! @} */

// ============================================================================
// BLE CALLBACK
// ============================================================================

/*!
 * \brief Callback for BLE data reception
 * \param data Received byte array
 */
static void prv_on_ble_data(const BleUartHal::Bytes &data) {
    BleMessage msg;
    msg.length = min(data.size(), sizeof(msg.data) - 1);
    for (size_t i = 0; i < msg.length; i++) {
        msg.data[i] = static_cast<char>(data[i]);
    }
    msg.data[msg.length] = '\0';
    xQueueSend(s_ctx.bleQueue, &msg, 0);
    Serial.printf("[BLE] RX: %s\n", msg.data);
}

// ============================================================================
// FSM HANDLERS
// ============================================================================

/*!
 * \brief Handle BOOT state
 */
static IoTState prv_handle_boot() {
    Serial.println("[FSM] Checking configuration...");

    if (!ConfigHandler::isConfigured()) {
        Serial.println("[FSM] Not configured, starting BLE advertising");
        if (s_ble)
            s_ble->startAdvertising_();
        return IoTState::BleAdvertising;
    }

    AppConfig cfg;
    if (ConfigHandler::load(cfg)) {
        s_ctx.deviceId = getDeviceIdFromConfig(cfg);
        Serial.printf("[FSM] Loaded Device ID: %d\n", s_ctx.deviceId);
    }

    Serial.println("[FSM] Already configured, connecting to WiFi");
    return IoTState::WifiConnecting;
}

/*!
 * \brief Handle BLE_ADVERTISING state
 */
static IoTState prv_handle_ble_advertising() {
    if (s_ble->isConnected()) {
        Serial.println("[BLE] Client connected");
        return IoTState::BleConfiguring;
    }
    return IoTState::BleAdvertising;
}

/*!
 * \brief Handle BLE_CONFIGURING state
 */
static IoTState prv_handle_ble_configuring() {
    if (!s_ble->isConnected()) {
        Serial.println("[BLE] Client disconnected");
        s_ble->startAdvertising_();
        return IoTState::BleAdvertising;
    }

    BleMessage msg;
    if (xQueueReceive(s_ctx.bleQueue, &msg, 0) == pdTRUE) {
        auto result = s_bleProtocol->handleCommand(msg.data, s_ctx.deviceId);

        // Update context from command result
        if (result.hasConfig) {
            s_ctx.hasPendingConfig = true;
            s_ctx.pendingConfig = result.config;
            s_ctx.deviceId = getDeviceIdFromConfig(result.config);
        } else if (!result.pendingCmd.isEmpty()) {
            // test_wifi case - has config in result but hasConfig is false
            s_ctx.hasPendingConfig = false;
            s_ctx.pendingConfig = result.config;
        }
        s_ctx.pendingCmd = result.pendingCmd;

        return result.nextState;
    }

    return IoTState::BleConfiguring;
}

/*!
 * \brief Handle BLE_TESTING_WIFI state
 */
static IoTState prv_handle_ble_testing_wifi() {
    // Initialize test
    if (s_ctx.testWifi == nullptr) {
        s_ctx.wifiTestStart = millis();
        s_ctx.lastProgressSent = -1;
        s_ctx.testWifi = new WiFiHal(
            s_ctx.pendingConfig.ssid.c_str(),
            s_ctx.pendingConfig.password.c_str());
        s_ctx.testWifi->begin();
        Serial.printf("[WIFI] Testing connection to: %s\n",
                      s_ctx.pendingConfig.ssid.c_str());
    }

    uint32_t elapsed = millis() - s_ctx.wifiTestStart;
    int progress = min(static_cast<int>(elapsed * 100 / IOT_WIFI_TEST_TIMEOUT_MS), 99);

    // Send progress every 20%
    if (progress / 20 != s_ctx.lastProgressSent / 20) {
        s_bleProtocol->sendStatus("connecting_wifi", progress);
        s_ctx.lastProgressSent = progress;
    }

    // Check success
    if (s_ctx.testWifi->isConnected()) {
        Serial.println("[WIFI] Test successful!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());

        s_bleProtocol->sendStatus("wifi_connected", 100);
        s_bleProtocol->sendResult(s_ctx.pendingCmd.c_str(), true);

        delete s_ctx.testWifi;
        s_ctx.testWifi = nullptr;

        // If config command, proceed to MQTT
        if (s_ctx.hasPendingConfig) {
            vTaskDelay(pdMS_TO_TICKS(500));

            Serial.println("[CONFIG] Writing config to flash");
            ConfigHandler::save(s_ctx.pendingConfig);

            // Create persistent WiFi manager (reuse connection)
            s_wifi = new WiFiHal(
                s_ctx.pendingConfig.ssid.c_str(),
                s_ctx.pendingConfig.password.c_str());

            vTaskDelay(pdMS_TO_TICKS(200));

            // Cleanup BLE
            delete s_bleProtocol;
            s_bleProtocol = nullptr;
            delete s_ble;
            s_ble = nullptr;

            s_ctx.hasPendingConfig = false;
            return IoTState::WifiConnecting;
        }

        // Just a test - return to configuring
        vTaskDelay(pdMS_TO_TICKS(500));
        WiFi.disconnect();
        return IoTState::BleConfiguring;
    }

    // Check timeout
    if (elapsed > IOT_WIFI_TEST_TIMEOUT_MS) {
        Serial.println("[WIFI] Test timeout");

        s_bleProtocol->sendResult(s_ctx.pendingCmd.c_str(), false, "wifi_timeout", "Connessione WiFi fallita");

        delete s_ctx.testWifi;
        s_ctx.testWifi = nullptr;
        WiFi.disconnect();

        if (s_ctx.hasPendingConfig) {
            ConfigHandler::clear();
            s_ctx.hasPendingConfig = false;
        }

        return IoTState::BleConfiguring;
    }

    return IoTState::BleTestingWifi;
}

/*!
 * \brief Handle WIFI_CONNECTING state
 */
static IoTState prv_handle_wifi_connecting() {
    AppConfig cfg;

    if (!ConfigHandler::load(cfg)) {
        s_ctx.configLoadFailures++;
        Serial.printf("[CONFIG] Load failed (%lu/%lu)\n",
                      s_ctx.configLoadFailures,
                      IOT_MAX_CONFIG_LOAD_FAILS);

        if (s_ctx.configLoadFailures >= IOT_MAX_CONFIG_LOAD_FAILS) {
            Serial.println("[CONFIG] Too many failures, clearing config");
            ConfigHandler::clear();
            s_ctx.configLoadFailures = 0;

            // Reinitialize BLE for reconfiguration
            if (!s_ble) {
                s_ble = new BleUartHal();
                s_ble->begin("PlantMonitor");
                s_ble->setRxHandler(prv_on_ble_data);
                s_bleProtocol = new BleProtocolHandler(s_ble);
            }
            s_ble->startAdvertising_();
            return IoTState::BleAdvertising;
        }

        vTaskDelay(pdMS_TO_TICKS(IOT_RECONNECT_DELAY_MS));
        return IoTState::WifiConnecting;
    }

    s_ctx.configLoadFailures = 0;
    s_ctx.deviceId = getDeviceIdFromConfig(cfg);

    if (!s_wifi) {
        Serial.printf("[WIFI] Connecting to: %s\n", cfg.ssid.c_str());
        s_wifi = new WiFiHal(cfg.ssid.c_str(), cfg.password.c_str());
        s_wifi->begin();
        s_ctx.wifiConnectStart = millis();
    }

    if (s_wifi->isConnected()) {
        Serial.println("[WIFI] Connected!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());

        // Configure NTP for time synchronization (needed for light tracking)
        if (!s_ctx.ntpConfigured) {
            Serial.println("[NTP] Configuring time synchronization...");
            // Configure for Italy timezone (CET-1CEST, UTC+1 with DST)
            configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
            setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
            tzset();
            s_ctx.ntpConfigured = true;
            Serial.println("[NTP] Time sync configured (will sync in background)");
        }

        s_ctx.wifiConnectStart = 0;
        return IoTState::MqttOperating;
    }

    s_wifi->begin();
    vTaskDelay(pdMS_TO_TICKS(5000));

    return IoTState::WifiConnecting;
}

/*!
 * \brief Handle MQTT_OPERATING state
 */
static IoTState prv_handle_mqtt_operating() {
    // Check WiFi connection
    if (s_wifi && !s_wifi->isConnected()) {
        Serial.println("[WIFI] Connection lost");
        delete s_wifi;
        s_wifi = nullptr;

        if (s_mqtt) {
            delete s_mqtt;
            s_mqtt = nullptr;
        }

        // Reset NTP flag so it will be reconfigured on reconnect
        s_ctx.ntpConfigured = false;

        return IoTState::WifiConnecting;
    }

    // Initialize MQTT if needed
    if (!s_mqtt) {
        s_mqtt = new MqttTelemetryPublisher(
            MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD, HIVEMQ_ROOT_CA);
    }

    if (!s_mqtt->isConnected()) {
        if (!s_mqtt->initialize()) {
            s_ctx.mqttInitRetries++;

            if (s_ctx.mqttInitRetries >= IOT_MAX_MQTT_INIT_RETRIES) {
                Serial.println("[MQTT] Max retries reached");
                s_ctx.mqttInitRetries = 0;

                delete s_wifi;
                s_wifi = nullptr;
                delete s_mqtt;
                s_mqtt = nullptr;

                return IoTState::WifiConnecting;
            }

            Serial.printf("[MQTT] Init failed, retry %lu/%lu\n",
                          s_ctx.mqttInitRetries,
                          IOT_MAX_MQTT_INIT_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(IOT_RECONNECT_DELAY_MS));
            return IoTState::MqttOperating;
        }

        s_ctx.mqttInitRetries = 0;
        s_ctx.firstMqttPublish = true;  // Force immediate publish after connection
        Serial.println("[MQTT] Connected!");
    }

    s_mqtt->poll();

    // Publish telemetry: immediately after connection, then periodically
    uint32_t now = millis();
    bool shouldPublish = s_ctx.firstMqttPublish ||
                        (now - s_ctx.lastMqttPublish >= IOT_MQTT_PUB_INTERVAL_MS);

    if (shouldPublish) {
        s_ctx.lastMqttPublish = now;
        s_ctx.firstMqttPublish = false;

        SensorData data;
        if (getLatestSensorData(data)) {
            s_mqtt->publishTelemetry(s_ctx.deviceId, data);
            Serial.println("[MQTT] Telemetry published");
        } else {
            Serial.println("[MQTT] Sensor data unavailable");
        }
    }

    return IoTState::MqttOperating;
}

/*!
 * \brief Handle ERROR state
 */
static IoTState prv_handle_error() {
    Serial.println("[FSM] Error state - attempting recovery");
    vTaskDelay(pdMS_TO_TICKS(5000));
    return IoTState::Boot;
}

// ============================================================================
// MAIN TASK
// ============================================================================

/*!
 * \brief Main IoT task function
 */
static void prv_iot_task(void *) {
    // Remove from watchdog (WiFi/TLS operations can be slow)
    esp_task_wdt_delete(NULL);

    // Initialize context
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.currentState = IoTState::Boot;
    s_ctx.deviceId = 1;
    s_ctx.testWifi = nullptr;
    s_ctx.lastProgressSent = -1;
    s_ctx.firstMqttPublish = true;
    s_ctx.ntpConfigured = false;

    // Create BLE message queue
    s_ctx.bleQueue = xQueueCreate(5, sizeof(BleMessage));
    if (!s_ctx.bleQueue) {
        Serial.println("[FSM] Failed to create BLE queue");
        vTaskDelete(nullptr);
        return;
    }

    // Initialize BLE
    s_ble = new BleUartHal();
    s_ble->begin("PlantMonitor");
    s_ble->setRxHandler(prv_on_ble_data);
    s_bleProtocol = new BleProtocolHandler(s_ble);

    Serial.println("[FSM] IoT Task started");
    Serial.printf("[FSM] Firmware: %s\n", IOT_FW_VERSION);

    // FSM main loop
    while (true) {
        IoTState current = s_ctx.currentState;
        IoTState next = current;

        switch (current) {
            case IoTState::Boot:
                next = prv_handle_boot();
                break;
            case IoTState::BleAdvertising:
                next = prv_handle_ble_advertising();
                break;
            case IoTState::BleConfiguring:
                next = prv_handle_ble_configuring();
                break;
            case IoTState::BleTestingWifi:
                next = prv_handle_ble_testing_wifi();
                break;
            case IoTState::WifiConnecting:
                next = prv_handle_wifi_connecting();
                break;
            case IoTState::MqttOperating:
                next = prv_handle_mqtt_operating();
                break;
            case IoTState::Error:
                next = prv_handle_error();
                break;
        }

        if (current != next) {
            Serial.printf("[FSM] %s -> %s\n",
                          iotStateToString(current),
                          iotStateToString(next));
        }
        s_ctx.currentState = next;

        vTaskDelay(pdMS_TO_TICKS(IOT_FSM_TICK_MS));
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void startIoTTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    xTaskCreatePinnedToCore(prv_iot_task, "IoTTask", stackSize, nullptr, priority, nullptr, core);
}

} // namespace Tasks
} // namespace PlantMonitor
