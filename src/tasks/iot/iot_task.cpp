#include "iot_task.h"

#include "drivers/bluetooth/bluetooth_hal.h"
#include "drivers/wifi/wifi_hal.h"
#include "iot/mqtt_service.h"
#include "iot/hivemq_ca.h"
#include "utils/configuration/private_data.h"
#include "utils/configuration/config.h"

#include <WiFiClientSecure.h>

using namespace PlantMonitor::IoT;
using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

/*! \defgroup Timing Timing Configurations
 *  @{
 */
#define MQTT_PUB_INTERVAL_MS    5000
#define RECONNECT_DELAY_MS      1000
#define FSM_TICK_MS             20
#define WIFI_TIMEOUT_MS         30000
#define MAX_MQTT_INIT_RETRIES   3
#define MAX_CONFIG_LOAD_FAILS   5
/*! @} */

// ============================================================================
// FSM DEFINITIONS
// ============================================================================

/*!
 * \brief System finite state machine states
 */
enum class SystemState {
    BOOT,
    BLE_ADVERTISING,
    BLE_CONFIGURING,
    WIFI_CONNECTING,
    MQTT_OPERATING,
    ERROR_STATE
};

/*!
 * \brief Global system context
 */
struct SystemContext {
    SystemState currentState;
    int deviceId;
    uint32_t lastMqttPublish;
    uint32_t wifiConnectStart;
    uint32_t mqttInitRetries;
    uint32_t configLoadFailures;
    SensorData sensorReadings;
    QueueHandle_t bleQueue;
};

/*!
 * \brief BLE message structure for queue
 */
struct BleMessage {
    char data[256];
    size_t length;
};

// ============================================================================
// STATIC STATE
// ============================================================================

static SystemContext systemCtx;

static BleUartHal* bleController = nullptr;
static WiFiHal* wifiManager = nullptr;
static WiFiClientSecure* tlsClient = nullptr;
static MqttService* mqttService = nullptr;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/*!
 * \brief Convert system state to string for logging
 * \param state Current state
 * \return State name as string
 */
static const char* stateToString(SystemState state) {
    switch (state) {
        case SystemState::BOOT:            return "BOOT";
        case SystemState::BLE_ADVERTISING: return "BLE_ADV";
        case SystemState::BLE_CONFIGURING: return "BLE_CFG";
        case SystemState::WIFI_CONNECTING: return "WIFI_CONN";
        case SystemState::MQTT_OPERATING:  return "MQTT_OP";
        case SystemState::ERROR_STATE:     return "ERROR";
        default:                           return "UNKNOWN";
    }
}

/*!
 * \brief Log state transition
 */
static void logStateTransition(SystemState from, SystemState to) {
    if (from != to) {
        Serial.printf("[FSM] %s -> %s\n", stateToString(from), stateToString(to));
    }
}

/*!
 * \brief Generate MQTT topic for device
 * \param deviceID Unique device identifier
 * \return Formatted topic string
 */
static String generateDeviceTopic(int deviceID) {
    char topic[64];
    snprintf(topic, sizeof(topic), "plantform/esp32_%03d/telemetry", deviceID);
    return String(topic);
}

/*!
 * \brief Create JSON telemetry payload
 */
static String createTelemetryJson(const String& status,
                                  float temperature,
                                  float humidity,
                                  float moisture,
                                  bool light) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"status\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"moisture\":%.2f,\"light\":%s}",
        status.c_str(), temperature, humidity, moisture, light ? "true" : "false"
    );
    return String(json);
}

// ============================================================================
// BLE CALLBACK
// ============================================================================

/*!
 * \brief BLE data received callback (called from BLE task context)
 * \param data Received bytes
 */
static void onBleDataReceived(const BleUartHal::Bytes& data) {
    BleMessage msg;
    msg.length = min(data.size(), sizeof(msg.data) - 1);
    
    for (size_t i = 0; i < msg.length; i++) {
        msg.data[i] = char(data[i]);
    }
    msg.data[msg.length] = '\0';
    
    // Thread-safe queue send
    xQueueSend(systemCtx.bleQueue, &msg, 0);
    
    Serial.printf("[BLE] RX: %s\n", msg.data);
}

// ============================================================================
// MQTT
// ============================================================================

/*!
 * \brief Initialize MQTT service with TLS
 * \return true if successful, false otherwise
 */
static bool initializeMqttService() {
    if (mqttService && mqttService->isConnected()) {
        return true;
    }
    
    // Create TLS client if not exists
    if (!tlsClient) {
        tlsClient = new WiFiClientSecure();
        tlsClient->setCACert(HIVEMQ_ROOT_CA);
        
        // Test TLS connection
        Serial.println("[MQTT] Testing TLS connection...");
        if (!tlsClient->connect(MQTT_BROKER, MQTT_PORT)) {
            Serial.println("[MQTT] TLS test failed");
            delete tlsClient;
            tlsClient = nullptr;
            return false;
        }
        tlsClient->stop();
        Serial.println("[MQTT] TLS test OK");
    }
    
    // Create MQTT service if not exists
    if (!mqttService) {
        mqttService = new MqttService(
            tlsClient,
            MQTT_BROKER,
            MQTT_PORT,
            MQTT_USER,
            MQTT_PASSWORD
        );
        
        mqttService->setMessageCallback([](String topic, String payload) {
            Serial.printf("[MQTT] RX %s: %s\n", topic.c_str(), payload.c_str());
        });
    }
    
    return mqttService->begin();
}

/*!
 * \brief Publish telemetry data to MQTT broker
 * \return true if successful
 */
static bool publishTelemetry() {
    if (!mqttService || !mqttService->isConnected()) {
        return false;
    }
    
    if (!getLatestSensorData(systemCtx.sensorReadings)) {
        Serial.println("[MQTT] Sensor data unavailable");
        return false;
    }
    
    String topic = generateDeviceTopic(systemCtx.deviceId);
    String msg = createTelemetryJson(
        "ok",
        systemCtx.sensorReadings.temperature,
        systemCtx.sensorReadings.humidity,
        systemCtx.sensorReadings.moisture,
        systemCtx.sensorReadings.lightDetected
    );
    
    bool success = mqttService->publish(topic.c_str(), msg.c_str(), false);
    if (success) {
        Serial.printf("[MQTT] Published to %s\n", topic.c_str());
    }
    
    return success;
}

// ============================================================================
// FSM HANDLERS
// ============================================================================

/*!
 * \brief Handle BOOT state - check if configured
 */
static SystemState handleBoot() {
    Serial.println("[FSM] Checking configuration...");
    
    if (!ConfigHandler::isConfigured()) {
        Serial.println("[FSM] Not configured, starting BLE");
        bleController->startAdvertising_();
        return SystemState::BLE_ADVERTISING;
    }
    
    Serial.println("[FSM] Already configured, connecting to WiFi");
    return SystemState::WIFI_CONNECTING;
}

/*!
 * \brief Handle BLE_ADVERTISING state - wait for connection
 */
static SystemState handleBleAdvertising() {
    if (bleController->isConnected()) {
        Serial.println("[BLE] Client connected");
        return SystemState::BLE_CONFIGURING;
    }
    return SystemState::BLE_ADVERTISING;
}

/*!
 * \brief Handle BLE_CONFIGURING state - receive and parse config
 */
static SystemState handleBleConfiguring() {
    if (!bleController->isConnected()) {
        Serial.println("[BLE] Client disconnected");
        bleController->startAdvertising_();
        return SystemState::BLE_ADVERTISING;
    }
    
    BleMessage msg;
    if (xQueueReceive(systemCtx.bleQueue, &msg, 0) == pdTRUE) {
        Serial.printf("[BLE] Processing config: %s\n", msg.data);
        
        AppConfig cfg;
        if (ConfigHandler::parseAppCfg(msg.data, cfg)) {
            if (ConfigHandler::save(cfg)) {
                Serial.println("[CONFIG] Saved successfully");
                bleController->end();
                return SystemState::WIFI_CONNECTING;
            } else {
                Serial.println("[CONFIG] Save failed");
            }
        } else {
            Serial.println("[CONFIG] Parse failed");
        }
    }
    
    return SystemState::BLE_CONFIGURING;
}

/*!
 * \brief Handle WIFI_CONNECTING state - connect to WiFi with timeout
 */
static SystemState handleWifiConnecting() {
    AppConfig cfg;
    
    // Try to load configuration
    if (!ConfigHandler::load(cfg)) {
        systemCtx.configLoadFailures++;
        Serial.printf("[CONFIG] Load failed (%d/%d)\n", 
            systemCtx.configLoadFailures, MAX_CONFIG_LOAD_FAILS);
        
        if (systemCtx.configLoadFailures >= MAX_CONFIG_LOAD_FAILS) {
            Serial.println("[CONFIG] Too many failures, clearing config");
            ConfigHandler::clear();
            systemCtx.configLoadFailures = 0;
            return SystemState::BLE_ADVERTISING;
        }
        
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        return SystemState::WIFI_CONNECTING;
    }
    
    systemCtx.configLoadFailures = 0;
    
    // Initialize WiFi manager
    if (!wifiManager) {
        Serial.printf("[WIFI] Connecting to: %s\n", cfg.ssid.c_str());
        wifiManager = new WiFiHal(cfg.ssid.c_str(), cfg.password.c_str());
        wifiManager->begin();
        systemCtx.wifiConnectStart = millis();
    }
    
    // Check connection
    if (wifiManager->isConnected()) {
        Serial.println("[WIFI] Connected!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
        systemCtx.wifiConnectStart = 0;
        return SystemState::MQTT_OPERATING;
    }
    
    // Check timeout
    if (millis() - systemCtx.wifiConnectStart > WIFI_TIMEOUT_MS) {
        Serial.println("[WIFI] Connection timeout");
        delete wifiManager;
        wifiManager = nullptr;
        systemCtx.wifiConnectStart = 0;
        return SystemState::BLE_ADVERTISING;
    }
    
    return SystemState::WIFI_CONNECTING;
}

/*!
 * \brief Handle MQTT_OPERATING state - maintain connection and publish
 */
static SystemState handleMqttOperating() {
    // Check WiFi connection first
    if (wifiManager && !wifiManager->isConnected()) {
        Serial.println("[WIFI] Connection lost");
        delete wifiManager;
        wifiManager = nullptr;
        
        if (mqttService) {
            delete mqttService;
            mqttService = nullptr;
        }
        
        return SystemState::WIFI_CONNECTING;
    }
    
    // Initialize or reconnect MQTT
    if (!mqttService || !mqttService->isConnected()) {
        if (!initializeMqttService()) {
            systemCtx.mqttInitRetries++;
            
            if (systemCtx.mqttInitRetries >= MAX_MQTT_INIT_RETRIES) {
                Serial.println("[MQTT] Max retries reached, reconnecting WiFi");
                systemCtx.mqttInitRetries = 0;
                
                delete wifiManager;
                wifiManager = nullptr;
                
                if (mqttService) {
                    delete mqttService;
                    mqttService = nullptr;
                }
                if (tlsClient) {
                    delete tlsClient;
                    tlsClient = nullptr;
                }
                
                return SystemState::WIFI_CONNECTING;
            }
            
            Serial.printf("[MQTT] Init failed, retry %d/%d\n", 
                systemCtx.mqttInitRetries, MAX_MQTT_INIT_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            return SystemState::MQTT_OPERATING;
        }
        
        systemCtx.mqttInitRetries = 0;
        Serial.println("[MQTT] Connected!");
    }
    
    // Poll MQTT client
    mqttService->poll();
    
    // Periodic telemetry publishing
    uint32_t now = millis();
    if (now - systemCtx.lastMqttPublish >= MQTT_PUB_INTERVAL_MS) {
        systemCtx.lastMqttPublish = now;
        publishTelemetry();
    }
    
    return SystemState::MQTT_OPERATING;
}

/*!
 * \brief Handle ERROR_STATE - log and attempt recovery
 */
static SystemState handleError() {
    Serial.println("[FSM] Error state - attempting recovery");
    vTaskDelay(pdMS_TO_TICKS(5000));
    return SystemState::BOOT;
}

// ============================================================================
// TASK LOOP
// ============================================================================

/*!
 * \brief Main IoT task loop
 */
static void IoTTask(void*) {
    // Initialize context
    systemCtx = {};
    systemCtx.currentState = SystemState::BOOT;
    systemCtx.deviceId = 1;
    
    // Create BLE message queue
    systemCtx.bleQueue = xQueueCreate(5, sizeof(BleMessage));
    if (!systemCtx.bleQueue) {
        Serial.println("[FSM] Failed to create BLE queue");
        vTaskDelete(nullptr);
        return;
    }
    
    // Initialize BLE
    bleController = new BleUartHal();
    bleController->begin("PlantMonitor");
    bleController->setRxHandler(onBleDataReceived);
    
    Serial.println("[FSM] IoT Task started");
    
    // Main state machine loop
    while (true) {
        SystemState currentState = systemCtx.currentState;
        SystemState nextState = currentState;
        
        switch (currentState) {
            case SystemState::BOOT:
                nextState = handleBoot();
                break;
                
            case SystemState::BLE_ADVERTISING:
                nextState = handleBleAdvertising();
                break;
                
            case SystemState::BLE_CONFIGURING:
                nextState = handleBleConfiguring();
                break;
                
            case SystemState::WIFI_CONNECTING:
                nextState = handleWifiConnecting();
                break;
                
            case SystemState::MQTT_OPERATING:
                nextState = handleMqttOperating();
                break;
                
            case SystemState::ERROR_STATE:
                nextState = handleError();
                break;
        }
        
        // Log state transitions
        logStateTransition(currentState, nextState);
        systemCtx.currentState = nextState;
        
        vTaskDelay(pdMS_TO_TICKS(FSM_TICK_MS));
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

/*!
 * \brief Start the IoT task
 * \param stackSize Task stack size in bytes
 * \param priority Task priority
 * \param core Core to pin the task to (-1 for no affinity)
 */
void startIoTTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    xTaskCreatePinnedToCore(
        IoTTask,
        "IoTTask",
        stackSize,
        nullptr,
        priority,
        nullptr,
        core
    );
}

} // namespace Tasks
} // namespace PlantMonitor