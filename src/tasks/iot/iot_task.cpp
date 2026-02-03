#include "iot_task.h"
// cspell:ignore MQTT mqtt Mqtt HIVEMQ plantform

#include "drivers/bluetooth/bluetooth_hal.h"
#include "drivers/wifi/wifi_hal.h"
#include "iot/mqtt_service.h"
#include "iot/hivemq_ca.h"
#include "utils/configuration/private_data.h"
#include "utils/configuration/config.h"

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

using namespace PlantMonitor::IoT;
using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

// ============================================================================
// CONSTANTS
// ============================================================================

#define MQTT_PUB_INTERVAL_MS    5000
#define RECONNECT_DELAY_MS      1000
#define FSM_TICK_MS             20
#define WIFI_TIMEOUT_MS         30000
#define WIFI_TEST_TIMEOUT_MS    15000
#define MAX_MQTT_INIT_RETRIES   3
#define MAX_CONFIG_LOAD_FAILS   5

#define FW_VERSION "1.0.0"
#define HW_VERSION "ESP32"

// Param indices (matching Flutter app)
enum ParamIndex {
    PARAM_PLANT_TYPE_ID = 0,
    PARAM_TEMP_MIN = 1,
    PARAM_TEMP_MAX = 2,
    PARAM_HUMIDITY_MIN = 3,
    PARAM_HUMIDITY_MAX = 4,
    PARAM_MOISTURE_MIN = 5,
    PARAM_MOISTURE_MAX = 6,
    PARAM_LIGHT_HOURS_MIN = 7,
    PARAM_DEVICE_ID = 8,
};

// ============================================================================
// FSM DEFINITIONS
// ============================================================================

enum class SystemState {
    BOOT,
    BLE_ADVERTISING,
    BLE_CONFIGURING,
    BLE_TESTING_WIFI,
    WIFI_CONNECTING,
    MQTT_OPERATING,
    ERROR_STATE
};

struct SystemContext {
    SystemState currentState;
    int deviceId;
    uint32_t lastMqttPublish;
    uint32_t wifiConnectStart;
    uint32_t mqttInitRetries;
    uint32_t configLoadFailures;
    SensorData sensorReadings;
    QueueHandle_t bleQueue;
    
    // Pending config durante test WiFi
    bool hasPendingConfig;
    AppConfig pendingConfig;
    String pendingCmd;
    
    // WiFi test state
    uint32_t wifiTestStart;
    WiFiHal* testWifi = nullptr;
    int lastProgressSent;
};

struct BleMessage {
    char data[512];
    size_t length;
};

// ============================================================================
// STATIC STATE
// ============================================================================

static SystemContext ctx;

static BleUartHal* bleController = nullptr;
static WiFiHal* wifiManager = nullptr;
static WiFiClientSecure* tlsClient = nullptr;
static MqttService* mqttService = nullptr;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static const char* stateToString(SystemState state) {
    switch (state) {
        case SystemState::BOOT:             return "BOOT";
        case SystemState::BLE_ADVERTISING:  return "BLE_ADV";
        case SystemState::BLE_CONFIGURING:  return "BLE_CFG";
        case SystemState::BLE_TESTING_WIFI: return "BLE_TEST_WIFI";
        case SystemState::WIFI_CONNECTING:  return "WIFI_CONN";
        case SystemState::MQTT_OPERATING:   return "MQTT_OP";
        case SystemState::ERROR_STATE:      return "ERROR";
        default:                            return "UNKNOWN";
    }
}

static void logStateTransition(SystemState from, SystemState to) {
    if (from != to) {
        Serial.printf("[FSM] %s -> %s\n", stateToString(from), stateToString(to));
    }
}

/// Helper per ottenere un param con valore di default
static float getParam(const AppConfig& cfg, size_t index, float defaultVal = 0.0f) {
    if (index < cfg.params.size()) {
        return cfg.params[index];
    }
    return defaultVal;
}

/// Helper per ottenere deviceId dalla config
static int getDeviceId(const AppConfig& cfg) {
    return static_cast<int>(getParam(cfg, PARAM_DEVICE_ID, 1.0f));
}

// ============================================================================
// BLE RESPONSE HELPERS
// ============================================================================

static void bleSendJson(JsonDocument& doc) {
    String json;
    serializeJson(doc, json);
    Serial.printf("[BLE] TX: %s\n", json.c_str());
    bleController->sendText(json.c_str());
}

static void bleSendJsonChunked(JsonDocument& doc) {
    String json;
    serializeJson(doc, json);
    Serial.printf("[BLE] TX (chunked): %s\n", json.c_str());
    bleController->sendTextChunked(json.c_str());
}

static void bleSendPong() {
    StaticJsonDocument<256> doc;
    doc["type"] = "pong";
    doc["fw_version"] = FW_VERSION;
    doc["hw_version"] = HW_VERSION;
    doc["configured"] = ConfigHandler::isConfigured();
    bleSendJson(doc);
}

static void bleSendInfo() {
    StaticJsonDocument<256> doc;
    doc["type"] = "info";
    doc["fw_version"] = FW_VERSION;
    doc["device_id"] = ctx.deviceId;
    doc["configured"] = ConfigHandler::isConfigured();
    
    AppConfig cfg;
    if (ConfigHandler::load(cfg)) {
        doc["wifi_ssid"] = cfg.ssid.c_str();
        if (!cfg.params.empty()) {
            doc["plant_type"] = static_cast<int>(getParam(cfg, PARAM_PLANT_TYPE_ID));
        }
    }
    
    bleSendJson(doc);
}

static void bleSendAck(const char* cmd) {
    StaticJsonDocument<128> doc;
    doc["type"] = "ack";
    doc["cmd"] = cmd;
    bleSendJson(doc);
}

static void bleSendStatus(const char* state, int progress = -1) {
    StaticJsonDocument<128> doc;
    doc["type"] = "status";
    doc["state"] = state;
    if (progress >= 0) {
        doc["progress"] = progress;
    }
    bleSendJson(doc);
}

static void bleSendResult(const char* cmd, bool success, const char* error = nullptr, const char* msg = nullptr) {
    StaticJsonDocument<256> doc;
    doc["type"] = "result";
    doc["cmd"] = cmd;
    doc["status"] = success ? "ok" : "error";
    if (error) doc["error"] = error;
    if (msg) doc["msg"] = msg;
    bleSendJson(doc);
}

static void bleSendWifiList() {
    auto networks = WiFiHal::scanNetworks(8);

    StaticJsonDocument<1024> doc;
    doc["type"] = "wifi_list";
    JsonArray arr = doc.createNestedArray("networks");

    for (const auto& net : networks) {
        JsonObject obj = arr.createNestedObject();
        obj["ssid"] = net.ssid;
        obj["rssi"] = net.rssi;
        obj["secure"] = net.secure;
    }

    bleSendJsonChunked(doc);
}

// ============================================================================
// BLE MESSAGE HANDLING
// ============================================================================

static void onBleDataReceived(const BleUartHal::Bytes& data) {
    BleMessage msg;
    msg.length = min(data.size(), sizeof(msg.data) - 1);
    
    for (size_t i = 0; i < msg.length; i++) {
        msg.data[i] = char(data[i]);
    }
    msg.data[msg.length] = '\0';
    
    xQueueSend(ctx.bleQueue, &msg, 0);
    Serial.printf("[BLE] RX: %s\n", msg.data);
}

static bool parseConfigFromJson(JsonDocument& doc, AppConfig& cfg) {
    if (!doc.containsKey("ssid") || !doc.containsKey("pass")) {
        return false;
    }
    
    cfg.ssid = doc["ssid"].as<const char*>();
    cfg.password = doc["pass"].as<const char*>();
    
    cfg.params.clear();
    
    if (doc.containsKey("params")) {
        JsonArray params = doc["params"].as<JsonArray>();
        for (JsonVariant v : params) {
            cfg.params.push_back(v.as<float>());
        }
    }
    
    return true;
}

static SystemState handleBleCommand(const char* data) {
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        Serial.printf("[BLE] JSON parse error: %s\n", error.c_str());
        bleSendResult("unknown", false, "parse_error", "JSON non valido");
        return SystemState::BLE_CONFIGURING;
    }
    
    const char* cmd = doc["cmd"] | "unknown";
    
    // PING
    if (strcmp(cmd, "ping") == 0) {
        bleSendPong();
        return SystemState::BLE_CONFIGURING;
    }
    // WIFI_SCAN
    else if (strcmp(cmd, "wifi_scan") == 0) {
        bleSendAck("wifi_scan");
        bleSendWifiList();
        return SystemState::BLE_CONFIGURING;
    }
    // GET_INFO
    else if (strcmp(cmd, "get_info") == 0) {
        bleSendInfo();
        return SystemState::BLE_CONFIGURING;
    }
    // CONFIG
    else if (strcmp(cmd, "config") == 0) {
        AppConfig cfg;
        if (parseConfigFromJson(doc, cfg)) {
            bleSendAck("config");
            bleSendStatus("saving_config");
            
            if (ConfigHandler::save(cfg)) {
                ctx.deviceId = getDeviceId(cfg);
                Serial.printf("[CONFIG] Saved. Device ID: %d\n", ctx.deviceId);
                
                // Test WiFi connection
                bleSendStatus("connecting_wifi", 0);
                ctx.pendingCmd = "config";
                ctx.hasPendingConfig = true;
                ctx.pendingConfig = cfg;
                return SystemState::BLE_TESTING_WIFI;
            } else {
                bleSendResult("config", false, "save_failed", "Saving Error NVS");
                return SystemState::BLE_CONFIGURING;
            }
        } else {
            bleSendResult("config", false, "invalid_params", "Missing parameters");
            return SystemState::BLE_CONFIGURING;
        }
    }
    // TEST_WIFI
    else if (strcmp(cmd, "test_wifi") == 0) {
        if (doc.containsKey("ssid") && doc.containsKey("pass")) {
            bleSendAck("test_wifi");
            
            ctx.pendingConfig.ssid = doc["ssid"].as<const char*>();
            ctx.pendingConfig.password = doc["pass"].as<const char*>();
            ctx.pendingConfig.params.clear();
            ctx.pendingCmd = "test_wifi";
            ctx.hasPendingConfig = false; // Don't save
            return SystemState::BLE_TESTING_WIFI;
        } else {
            bleSendResult("test_wifi", false, "invalid_params", "missing SSID or password");
            return SystemState::BLE_CONFIGURING;
        }
    }
    // RESET
    else if (strcmp(cmd, "reset") == 0) {
        bleSendAck("reset");
        ConfigHandler::clear();
        ctx.deviceId = 1;
        bleSendResult("reset", true);
        return SystemState::BLE_ADVERTISING;
    }
    // UNKNOWN
    else {
        bleSendResult(cmd, false, "unknown_cmd", "Unknown command");
        return SystemState::BLE_CONFIGURING;
    }
}

// ============================================================================
// MQTT FUNCTIONS
// ============================================================================

static String generateDeviceTopic(int deviceID) {
    char topic[64];
    snprintf(topic, sizeof(topic), "plantform/esp32_%03d/telemetry", deviceID);
    return String(topic);
}

static String createTelemetryJson(const String& status, float temperature, float humidity, float moisture, bool light) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"status\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"moisture\":%.2f,\"light\":%s,\"device_id\":%d}",
        status.c_str(), temperature, humidity, moisture, light ? "true" : "false", ctx.deviceId
    );
    return String(json);
}

static bool initializeMqttService() {
    if (mqttService && mqttService->isConnected()) {
        return true;
    }

    if (!tlsClient) {
        tlsClient = new WiFiClientSecure();
        tlsClient->setCACert(HIVEMQ_ROOT_CA);
        tlsClient->setTimeout(10);

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
    if (!mqttService) {
        mqttService = new MqttService(tlsClient, MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD);
        mqttService->setMessageCallback([](String topic, String payload) {
            Serial.printf("[MQTT] RX %s: %s\n", topic.c_str(), payload.c_str());
        });
    }

    return mqttService->begin();
}

static bool publishTelemetry() {
    if (!mqttService || !mqttService->isConnected()) {
        return false;
    }
    
    if (!getLatestSensorData(ctx.sensorReadings)) {
        Serial.println("[MQTT] Sensor data unavailable");
        return false;
    }
    
    String topic = generateDeviceTopic(ctx.deviceId);
    String msg = createTelemetryJson(
        "ok",
        ctx.sensorReadings.temperature,
        ctx.sensorReadings.humidity,
        ctx.sensorReadings.moisture,
        ctx.sensorReadings.lightDetected
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

static SystemState handleBoot() {
    Serial.println("[FSM] Checking configuration...");
    
    if (!ConfigHandler::isConfigured()) {
        Serial.println("[FSM] Not configured, starting BLE advertising");
        if (bleController) bleController->startAdvertising_();
        return SystemState::BLE_ADVERTISING;
    }
    
    // Carica deviceId dalla config
    AppConfig cfg;
    if (ConfigHandler::load(cfg)) {
        ctx.deviceId = getDeviceId(cfg);
        Serial.printf("[FSM] Loaded Device ID: %d\n", ctx.deviceId);
    }
    
    Serial.println("[FSM] Already configured, connecting to WiFi");
    return SystemState::WIFI_CONNECTING;
}

static SystemState handleBleAdvertising() {
    if (bleController->isConnected()) {
        Serial.println("[BLE] Client connected");
        return SystemState::BLE_CONFIGURING;
    }
    return SystemState::BLE_ADVERTISING;
}

static SystemState handleBleConfiguring() {

    // Check disconnection
    if (!bleController->isConnected()) {
        Serial.println("[BLE] Client disconnected");
        if (bleController) bleController->startAdvertising_();
        return SystemState::BLE_ADVERTISING;
    }
    
    // Process BLE messages
    BleMessage msg;
    if (xQueueReceive(ctx.bleQueue, &msg, 0) == pdTRUE) {
        return handleBleCommand(msg.data);
    }
    
    return SystemState::BLE_CONFIGURING;
}

static SystemState handleBleTestingWifi() {
    // Inizializza test
    if (ctx.testWifi == nullptr) {
        ctx.wifiTestStart = millis();
        ctx.lastProgressSent = -1;
        ctx.testWifi = new WiFiHal(ctx.pendingConfig.ssid.c_str(), ctx.pendingConfig.password.c_str());
        ctx.testWifi->begin();
        Serial.printf("[WIFI] Testing connection to: %s\n", ctx.pendingConfig.ssid.c_str());
    }
    
    // Aggiorna progresso
    uint32_t elapsed = millis() - ctx.wifiTestStart;
    int progress = min((int)(elapsed * 100 / WIFI_TEST_TIMEOUT_MS), 99);
    
    // Invia progress ogni 20%
    if (progress / 20 != ctx.lastProgressSent / 20) {
        bleSendStatus("connecting_wifi", progress);
        ctx.lastProgressSent = progress;
    }
    
    // Check success
    if (ctx.testWifi->isConnected()) {
        Serial.println("[WIFI] Test successful!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
        
        bleSendStatus("wifi_connected", 100);
        bleSendResult(ctx.pendingCmd.c_str(), true);

        // Cleanup test
        delete ctx.testWifi;
        ctx.testWifi = nullptr;
        
        // Se era config, vai a MQTT
        if (ctx.hasPendingConfig) {
            vTaskDelay(pdMS_TO_TICKS(500));
            
            // Crea wifiManager permanente (riusa connessione esistente)
            wifiManager = new WiFiHal(ctx.pendingConfig.ssid.c_str(), ctx.pendingConfig.password.c_str());
            // Non chiamare begin(), siamo già connessi

            vTaskDelay(pdMS_TO_TICKS(200));
            delete bleController;
            bleController = nullptr;
            
            ctx.hasPendingConfig = false;
            return SystemState::MQTT_OPERATING;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
        WiFi.disconnect();
        // Se era solo test, torna a configuring
        return SystemState::BLE_CONFIGURING;
    }
    
    // Check timeout
    if (elapsed > WIFI_TEST_TIMEOUT_MS) {
        Serial.println("[WIFI] Test timeout");
        
        bleSendResult(ctx.pendingCmd.c_str(), false, "wifi_timeout", "Connessione WiFi fallita");
        
        delete ctx.testWifi;
        ctx.testWifi = nullptr;
        WiFi.disconnect();
        
        // delete config if pending
        if (ctx.hasPendingConfig) {
            ConfigHandler::clear();
            ctx.hasPendingConfig = false;
        }
        
        return SystemState::BLE_CONFIGURING;
    }
    
    return SystemState::BLE_TESTING_WIFI;
}

static SystemState handleWifiConnecting() {
    AppConfig cfg;
    
    if (!ConfigHandler::load(cfg)) {
        ctx.configLoadFailures++;
        Serial.printf("[CONFIG] Load failed (%d/%d)\n", ctx.configLoadFailures, MAX_CONFIG_LOAD_FAILS);
        
        if (ctx.configLoadFailures >= MAX_CONFIG_LOAD_FAILS) {
            Serial.println("[CONFIG] Too many failures, clearing config");
            ConfigHandler::clear();
            ctx.configLoadFailures = 0;
            if (bleController) bleController->startAdvertising_();
            return SystemState::BLE_ADVERTISING;
        }
        
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        return SystemState::WIFI_CONNECTING;
    }
    
    ctx.configLoadFailures = 0;
    ctx.deviceId = getDeviceId(cfg);
    
    if (!wifiManager) {
        Serial.printf("[WIFI] Connecting to: %s\n", cfg.ssid.c_str());
        wifiManager = new WiFiHal(cfg.ssid.c_str(), cfg.password.c_str());
        wifiManager->begin();
        ctx.wifiConnectStart = millis();
    }
    
    if (wifiManager->isConnected()) {
        Serial.println("[WIFI] Connected!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
        ctx.wifiConnectStart = 0;

        return SystemState::MQTT_OPERATING;
    }
    
    wifiManager->begin();
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    return SystemState::WIFI_CONNECTING;
}

static SystemState handleMqttOperating() {
    if (wifiManager && !wifiManager->isConnected()) {
        Serial.println("[WIFI] Connection lost");
        delete wifiManager;
        wifiManager = nullptr;
        
        if (mqttService) {
            delete mqttService;
            mqttService = nullptr;
        }
        
        if (tlsClient){
            delete tlsClient;
            tlsClient = nullptr;
        }

        return SystemState::WIFI_CONNECTING;
    }
    
    if (!mqttService || !mqttService->isConnected()) {
        if (!initializeMqttService()) {
            ctx.mqttInitRetries++;
            
            if (ctx.mqttInitRetries >= MAX_MQTT_INIT_RETRIES) {
                Serial.println("[MQTT] Max retries reached");
                ctx.mqttInitRetries = 0;
                
                delete wifiManager;
                wifiManager = nullptr;
                
                if (mqttService) { delete mqttService; mqttService = nullptr; }
                if (tlsClient) { delete tlsClient; tlsClient = nullptr; }
                
                return SystemState::WIFI_CONNECTING;
            }
            
            Serial.printf("[MQTT] Init failed, retry %d/%d\n", ctx.mqttInitRetries, MAX_MQTT_INIT_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            return SystemState::MQTT_OPERATING;
        }
        
        ctx.mqttInitRetries = 0;
        Serial.println("[MQTT] Connected!");
    }
    
    mqttService->poll();
    
    uint32_t now = millis();
    if (now - ctx.lastMqttPublish >= MQTT_PUB_INTERVAL_MS) {
        ctx.lastMqttPublish = now;
        publishTelemetry();
    }
    
    return SystemState::MQTT_OPERATING;
}

static SystemState handleError() {
    Serial.println("[FSM] Error state - attempting recovery");
    vTaskDelay(pdMS_TO_TICKS(5000));
    return SystemState::BOOT;
}

// ============================================================================
// TASK
// ============================================================================

static void IoTTask(void*) {
    // Inizializza contesto
    memset(&ctx, 0, sizeof(ctx));
    ctx.currentState = SystemState::BOOT;
    ctx.deviceId = 1;
    ctx.testWifi = nullptr;
    ctx.lastProgressSent = -1;
    
    ctx.bleQueue = xQueueCreate(5, sizeof(BleMessage));
    if (!ctx.bleQueue) {
        Serial.println("[FSM] Failed to create BLE queue");
        vTaskDelete(nullptr);
        return;
    }
    
    bleController = new BleUartHal();
    bleController->begin("PlantMonitor");
    bleController->setRxHandler(onBleDataReceived);
    
    Serial.println("[FSM] IoT Task started");
    Serial.printf("[FSM] Firmware: %s\n", FW_VERSION);
    
    while (true) {
        SystemState currentState = ctx.currentState;
        SystemState nextState = currentState;
        
        switch (currentState) {
            case SystemState::BOOT:             nextState = handleBoot(); break;
            case SystemState::BLE_ADVERTISING:  nextState = handleBleAdvertising(); break;
            case SystemState::BLE_CONFIGURING:  nextState = handleBleConfiguring(); break;
            case SystemState::BLE_TESTING_WIFI: nextState = handleBleTestingWifi(); break;
            case SystemState::WIFI_CONNECTING:  nextState = handleWifiConnecting(); break;
            case SystemState::MQTT_OPERATING:   nextState = handleMqttOperating(); break;
            case SystemState::ERROR_STATE:      nextState = handleError(); break;
        }
        
        logStateTransition(currentState, nextState);
        ctx.currentState = nextState;
        
        vTaskDelay(pdMS_TO_TICKS(FSM_TICK_MS));
    }
}

void startIoTTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    xTaskCreatePinnedToCore(IoTTask, "IoTTask", stackSize, nullptr, priority, nullptr, core);
}

} // namespace Tasks
} // namespace PlantMonitor