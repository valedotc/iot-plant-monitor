#include "iot-task.h"
// cspell:ignore MQTT mqtt Mqtt HIVEMQ plantform

#include "drivers/bluetooth/bluetooth-hal.h"
#include "drivers/wifi/wifi-hal.h"
#include "iot/mqtt-service.h"
#include "iot/hivemq-ca.h"
#include "utils/configuration/private-data.h"
#include "utils/configuration/config.h"
#include "esp_task_wdt.h"

#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

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

// Param indices
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
    SYSTEM_STATE_BOOT,
    SYSTEM_STATE_BLE_ADVERTISING,
    SYSTEM_STATE_BLE_CONFIGURING,
    SYSTEM_STATE_BLE_TESTING_WIFI,
    SYSTEM_STATE_WIFI_CONNECTING,
    SYSTEM_STATE_MQTT_OPERATING,
    SYSTEM_STATE_ERROR
};

struct SystemContext {
    SystemState currentState;
    int deviceId;
    uint32_t wifiConnectStart;
    uint32_t mqttInitRetries;
    uint32_t configLoadFailures;
    SensorData sensorReadings;
    QueueHandle_t bleQueue;
    
    // Pending config during WiFi test
    bool hasPendingConfig;
    AppConfig pendingConfig;
    String pendingCmd;
    
    // WiFi test state
    uint32_t wifiTestStart;
    WiFiHal* testWifi = nullptr;
    int lastProgressSent;

    PlantMonitor::Utils::PeriodicSendTimer sendTimer;
};

struct BleMessage {
    char data[512];
    size_t length;
};

// ============================================================================
// STATIC STATE
// ============================================================================

static SystemContext iot_task_ctx;

static BleUartHal* iot_task_ble_controller = nullptr;
static WiFiHal* iot_task_wifi_manager = nullptr;
static WiFiClientSecure* iot_task_tls_client = nullptr;
static MqttService* iot_task_mqtt_service = nullptr;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static const char* prv_state_to_string(SystemState state) {
    switch (state) {
        case SystemState::SYSTEM_STATE_BOOT:             return "BOOT";
        case SystemState::SYSTEM_STATE_BLE_ADVERTISING:  return "BLE_ADV";
        case SystemState::SYSTEM_STATE_BLE_CONFIGURING:  return "BLE_CFG";
        case SystemState::SYSTEM_STATE_BLE_TESTING_WIFI: return "BLE_TEST_WIFI";
        case SystemState::SYSTEM_STATE_WIFI_CONNECTING:  return "WIFI_CONN";
        case SystemState::SYSTEM_STATE_MQTT_OPERATING:   return "MQTT_OP";
        case SystemState::SYSTEM_STATE_ERROR:      return "ERROR";
        default:                            return "UNKNOWN";
    }
}

static void prv_log_state_transition(SystemState from, SystemState to) {
    if (from != to) {
        Serial.printf("[FSM] %s -> %s\n", prv_state_to_string(from), prv_state_to_string(to));
    }
}

/// Helper per ottenere un param con valore di default
static float prv_get_param(const AppConfig& cfg, size_t index, float defaultVal = 0.0f) {
    if (index < cfg.params.size()) {
        return cfg.params[index];
    }
    return defaultVal;
}

/// Helper per ottenere deviceId dalla config
static int prv_get_device_id(const AppConfig& cfg) {
    return static_cast<int>(prv_get_param(cfg, PARAM_DEVICE_ID, 1.0f));
}

// ============================================================================
// BLE RESPONSE HELPERS
// ============================================================================

static void prv_ble_send_json(JsonDocument& doc) {
    String json;
    serializeJson(doc, json);
    Serial.printf("[BLE] TX: %s\n", json.c_str());
    iot_task_ble_controller->sendText(json.c_str());
}

static void prv_ble_send_jsonChunked(JsonDocument& doc) {
    String json;
    serializeJson(doc, json);
    Serial.printf("[BLE] TX (chunked): %s\n", json.c_str());
    iot_task_ble_controller->sendTextChunked(json.c_str());
}

static void prv_ble_send_pong() {
    StaticJsonDocument<256> doc;
    doc["type"] = "pong";
    doc["fw_version"] = FW_VERSION;
    doc["hw_version"] = HW_VERSION;
    doc["configured"] = ConfigHandler::isConfigured();
    prv_ble_send_json(doc);
}

static void prv_ble_send_info() {
    StaticJsonDocument<256> doc;
    doc["type"] = "info";
    doc["fw_version"] = FW_VERSION;
    doc["device_id"] = iot_task_ctx.deviceId;
    doc["configured"] = ConfigHandler::isConfigured();
    
    AppConfig cfg;
    if (ConfigHandler::load(cfg)) {
        doc["wifi_ssid"] = cfg.ssid.c_str();
        if (!cfg.params.empty()) {
            doc["plant_type"] = static_cast<int>(prv_get_param(cfg, PARAM_PLANT_TYPE_ID));
        }
    }
    
    prv_ble_send_json(doc);
}

static void prv_ble_send_ack(const char* cmd) {
    StaticJsonDocument<128> doc;
    doc["type"] = "ack";
    doc["cmd"] = cmd;
    prv_ble_send_json(doc);
}

static void prv_ble_send_status(const char* state, int progress = -1) {
    StaticJsonDocument<128> doc;
    doc["type"] = "status";
    doc["state"] = state;
    if (progress >= 0) {
        doc["progress"] = progress;
    }
    prv_ble_send_json(doc);
}

static void prv_ble_send_result(const char* cmd, bool success, const char* error = nullptr, const char* msg = nullptr) {
    StaticJsonDocument<256> doc;
    doc["type"] = "result";
    doc["cmd"] = cmd;
    doc["status"] = success ? "ok" : "error";
    if (error) doc["error"] = error;
    if (msg) doc["msg"] = msg;
    prv_ble_send_json(doc);
}

static void prv_ble_send_wifi_list() {
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

    prv_ble_send_jsonChunked(doc);
}

// ============================================================================
// BLE MESSAGE HANDLING
// ============================================================================

static void prv_on_ble_data_received(const BleUartHal::Bytes& data) {
    BleMessage msg;
    msg.length = min(data.size(), sizeof(msg.data) - 1);
    
    for (size_t i = 0; i < msg.length; i++) {
        msg.data[i] = char(data[i]);
    }
    msg.data[msg.length] = '\0';
    
    xQueueSend(iot_task_ctx.bleQueue, &msg, 0);
    Serial.printf("[BLE] RX: %s\n", msg.data);
}

static bool prv_parse_config_from_json(JsonDocument& doc, AppConfig& cfg) {
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

static SystemState prv_handle_ble_command(const char* data) {
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, data);
    
    if (error) {
        Serial.printf("[BLE] JSON parse error: %s\n", error.c_str());
        prv_ble_send_result("unknown", false, "parse_error", "JSON non valido");
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
    
    const char* cmd = doc["cmd"] | "unknown";
    
    // PING
    if (strcmp(cmd, "ping") == 0) {
        prv_ble_send_pong();
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
    // WIFI_SCAN
    else if (strcmp(cmd, "wifi_scan") == 0) {
        prv_ble_send_ack("wifi_scan");
        prv_ble_send_wifi_list();
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
    // GET_INFO
    else if (strcmp(cmd, "get_info") == 0) {
        prv_ble_send_info();
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
    // CONFIG
    else if (strcmp(cmd, "config") == 0) {
        AppConfig cfg;
        if (prv_parse_config_from_json(doc, cfg)) {
            prv_ble_send_ack("config");
            prv_ble_send_status("saving_config");
            
            // first check the credentials
            // then make a test fo connection.

            //if (ConfigHandler::save(cfg)) {
            iot_task_ctx.deviceId = prv_get_device_id(cfg);
            //Serial.printf("[CONFIG] Saved. Device ID: %d\n", iot_task_ctx.deviceId);
            
            // Test WiFi connection
            prv_ble_send_status("connecting_wifi", 0);
            iot_task_ctx.pendingCmd = "config";
            iot_task_ctx.hasPendingConfig = true;
            iot_task_ctx.pendingConfig = cfg;
            return SystemState::SYSTEM_STATE_BLE_TESTING_WIFI;
            //} else {
            //    prv_ble_send_result("config", false, "save_failed", "Saving Error NVS");
            //    return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
            //}
        } else {
            prv_ble_send_result("config", false, "invalid_params", "Missing parameters");
            return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
        }
    }
    // TEST_WIFI
    else if (strcmp(cmd, "test_wifi") == 0) {
        if (doc.containsKey("ssid") && doc.containsKey("pass")) {
            prv_ble_send_ack("test_wifi");
            
            iot_task_ctx.pendingConfig.ssid = doc["ssid"].as<const char*>();
            iot_task_ctx.pendingConfig.password = doc["pass"].as<const char*>();
            iot_task_ctx.pendingConfig.params.clear();
            iot_task_ctx.pendingCmd = "test_wifi";
            iot_task_ctx.hasPendingConfig = false; // Don't save
            return SystemState::SYSTEM_STATE_BLE_TESTING_WIFI;
        } else {
            prv_ble_send_result("test_wifi", false, "invalid_params", "missing SSID or password");
            return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
        }
    }
    // RESET
    else if (strcmp(cmd, "reset") == 0) {
        prv_ble_send_ack("reset");
        ConfigHandler::clear();
        iot_task_ctx.deviceId = 1;
        prv_ble_send_result("reset", true);
        return SystemState::SYSTEM_STATE_BLE_ADVERTISING;
    }
    // UNKNOWN
    else {
        prv_ble_send_result(cmd, false, "unknown_cmd", "Unknown command");
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
}

// ============================================================================
// MQTT FUNCTIONS
// ============================================================================

static String prv_generate_device_topic(int deviceID) {
    char topic[64];
    snprintf(topic, sizeof(topic), "plantform/esp32_%03d/telemetry", deviceID);
    return String(topic);
}

static String prv_create_telemetry_json(const String& status, float temperature, float humidity, float moisture, bool light) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"status\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"moisture\":%.2f,\"light\":%s,\"device_id\":%d}",
        status.c_str(), temperature, humidity, moisture, light ? "true" : "false", iot_task_ctx.deviceId
    );
    return String(json);
}

static bool prv_initialize_mqtt_service() {
    if (iot_task_mqtt_service && iot_task_mqtt_service->isConnected()) {
        return true;
    }

    if (!iot_task_tls_client) {
        iot_task_tls_client = new WiFiClientSecure();
        iot_task_tls_client->setCACert(HIVEMQ_ROOT_CA);

        Serial.println("[MQTT] Testing TLS connection...");

        //avoid triggering watchdog during TLS handshake
        esp_task_wdt_reset();

        if (!iot_task_tls_client->connect(MQTT_BROKER, MQTT_PORT)) {
            Serial.println("[MQTT] TLS test failed");
            delete iot_task_tls_client;
            iot_task_tls_client = nullptr;
            return false;
        }
        iot_task_tls_client->stop();
        Serial.println("[MQTT] TLS test OK");
    }
    if (!iot_task_mqtt_service) {
        iot_task_mqtt_service = new MqttService(iot_task_tls_client, MQTT_BROKER, MQTT_PORT, MQTT_USER, MQTT_PASSWORD);
        iot_task_mqtt_service->setMessageCallback([](String topic, String payload) {
            Serial.printf("[MQTT] RX %s: %s\n", topic.c_str(), payload.c_str());
        });
    }

    return iot_task_mqtt_service->begin();
}

static bool prv_publish_telemetry() {
    if (!iot_task_mqtt_service || !iot_task_mqtt_service->isConnected()) {
        return false;
    }
    
    if (!getLatestSensorData(iot_task_ctx.sensorReadings)) {
        Serial.println("[MQTT] Sensor data unavailable");
        return false;
    }
    
    String topic = prv_generate_device_topic(iot_task_ctx.deviceId);
    String msg = prv_create_telemetry_json(
        "ok",
        iot_task_ctx.sensorReadings.temperature,
        iot_task_ctx.sensorReadings.humidity,
        iot_task_ctx.sensorReadings.moisture,
        iot_task_ctx.sensorReadings.lightDetected
    );
    
    bool success = iot_task_mqtt_service->publish(topic.c_str(), msg.c_str(), false);
    if (success) {
        Serial.printf("[MQTT] Published to %s\n", topic.c_str());
    }
    
    return success;
}

// ============================================================================
// FSM HANDLERS
// ============================================================================

static SystemState prv_handle_boot() {
    Serial.println("[FSM] Checking configuration...");
    
    if (!ConfigHandler::isConfigured()) {
        Serial.println("[FSM] Not configured, starting BLE advertising");
        if (iot_task_ble_controller) iot_task_ble_controller->startAdvertising_();
        return SystemState::SYSTEM_STATE_BLE_ADVERTISING;
    }
    
    // Carica deviceId dalla config
    AppConfig cfg;
    if (ConfigHandler::load(cfg)) {
        iot_task_ctx.deviceId = prv_get_device_id(cfg);
        Serial.printf("[FSM] Loaded Device ID: %d\n", iot_task_ctx.deviceId);
    }
    
    Serial.println("[FSM] Already configured, connecting to WiFi");
    return SystemState::SYSTEM_STATE_WIFI_CONNECTING;
}

static SystemState prv_handle_ble_advertising() {
    if (iot_task_ble_controller->isConnected()) {
        Serial.println("[BLE] Client connected");
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
    return SystemState::SYSTEM_STATE_BLE_ADVERTISING;
}

static SystemState prv_handle_ble_configuring() {

    // Check disconnection
    if (!iot_task_ble_controller->isConnected()) {
        Serial.println("[BLE] Client disconnected");
        if (iot_task_ble_controller) iot_task_ble_controller->startAdvertising_();
        return SystemState::SYSTEM_STATE_BLE_ADVERTISING;
    }
    
    // Process BLE messages
    BleMessage msg;
    if (xQueueReceive(iot_task_ctx.bleQueue, &msg, 0) == pdTRUE) {
        return prv_handle_ble_command(msg.data);
    }
    
    return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
}

static SystemState prv_handle_ble_testing_wifi() {
    // Initialize test
    if (iot_task_ctx.testWifi == nullptr) {
        iot_task_ctx.wifiTestStart = millis();
        iot_task_ctx.lastProgressSent = -1;
        iot_task_ctx.testWifi = new WiFiHal(iot_task_ctx.pendingConfig.ssid.c_str(), iot_task_ctx.pendingConfig.password.c_str());
        iot_task_ctx.testWifi->begin();
        Serial.printf("[WIFI] Testing connection to: %s\n", iot_task_ctx.pendingConfig.ssid.c_str());
    }
    
    // Aggiorna progresso
    uint32_t elapsed = millis() - iot_task_ctx.wifiTestStart;
    int progress = min((int)(elapsed * 100 / WIFI_TEST_TIMEOUT_MS), 99);
    
    // Invia progress ogni 20%
    if (progress / 20 != iot_task_ctx.lastProgressSent / 20) {
        prv_ble_send_status("connecting_wifi", progress);
        iot_task_ctx.lastProgressSent = progress;
    }
    
    // Check success
    if (iot_task_ctx.testWifi->isConnected()) {

        //SUCCESS TEST
        Serial.println("[WIFI] Test successful!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
        
        prv_ble_send_status("wifi_connected", 100);
        prv_ble_send_result(iot_task_ctx.pendingCmd.c_str(), true);
        
        // Cleanup test
        delete iot_task_ctx.testWifi;
        iot_task_ctx.testWifi = nullptr;
    
        // Se era config, vai a MQTT
        if (iot_task_ctx.hasPendingConfig) {
            vTaskDelay(pdMS_TO_TICKS(500));
            
            AppConfig cfg;
            cfg = iot_task_ctx.pendingConfig;
            Serial.println("[CONFIG] WROTE CONFIG ON FLASH");
            ConfigHandler::save(cfg);
            iot_task_ctx.hasPendingConfig = false;

            // Crea iot_task_wifi_manager permanente (riusa connessione esistente)
            iot_task_wifi_manager = new WiFiHal(iot_task_ctx.pendingConfig.ssid.c_str(), iot_task_ctx.pendingConfig.password.c_str());
            // Non chiamare begin(), siamo già connessi
            
            vTaskDelay(pdMS_TO_TICKS(200));
            delete iot_task_ble_controller;
            iot_task_ble_controller = nullptr;
            
            iot_task_ctx.hasPendingConfig = false;
            return SystemState::SYSTEM_STATE_WIFI_CONNECTING;
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
        WiFi.disconnect();
        // Se era solo test, torna a configuring
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
    
    // Check timeout
    if (elapsed > WIFI_TEST_TIMEOUT_MS) {
        Serial.println("[WIFI] Test timeout");
        
        prv_ble_send_result(iot_task_ctx.pendingCmd.c_str(), false, "wifi_timeout", "Connessione WiFi fallita");
        
        delete iot_task_ctx.testWifi;
        iot_task_ctx.testWifi = nullptr;
        WiFi.disconnect();
        
        // delete config if pending
        if (iot_task_ctx.hasPendingConfig) {
            ConfigHandler::clear();
            iot_task_ctx.hasPendingConfig = false;
        }
        
        return SystemState::SYSTEM_STATE_BLE_CONFIGURING;
    }
    
    return SystemState::SYSTEM_STATE_BLE_TESTING_WIFI;
}

static SystemState prv_handle_wifi_connecting() {
    AppConfig cfg;
    
    if (!ConfigHandler::load(cfg)) {
        iot_task_ctx.configLoadFailures++;
        Serial.printf("[CONFIG] Load failed (%d/%d)\n", iot_task_ctx.configLoadFailures, MAX_CONFIG_LOAD_FAILS);
        
        if (iot_task_ctx.configLoadFailures >= MAX_CONFIG_LOAD_FAILS) {
            Serial.println("[CONFIG] Too many failures, clearing config");
            ConfigHandler::clear();
            iot_task_ctx.configLoadFailures = 0;
            if (iot_task_ble_controller) iot_task_ble_controller->startAdvertising_();
            return SystemState::SYSTEM_STATE_BLE_ADVERTISING;
        }
        
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        return SystemState::SYSTEM_STATE_WIFI_CONNECTING;
    }
    
    iot_task_ctx.configLoadFailures = 0;
    iot_task_ctx.deviceId = prv_get_device_id(cfg);
    
    if (!iot_task_wifi_manager) {
        Serial.printf("[WIFI] Connecting to: %s\n", cfg.ssid.c_str());
        iot_task_wifi_manager = new WiFiHal(cfg.ssid.c_str(), cfg.password.c_str());
        iot_task_wifi_manager->begin();
        iot_task_ctx.wifiConnectStart = millis();
    }
    
    if (iot_task_wifi_manager->isConnected()) {
        Serial.println("[WIFI] Connected!");
        Serial.printf("[WIFI] IP: %s\n", WiFi.localIP().toString().c_str());
        iot_task_ctx.wifiConnectStart = 0;

        return SystemState::SYSTEM_STATE_MQTT_OPERATING;
    }
    
    iot_task_wifi_manager->begin();
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    return SystemState::SYSTEM_STATE_WIFI_CONNECTING;
}

static SystemState prv_handle_mqtt_operating() {
    if (iot_task_wifi_manager && !iot_task_wifi_manager->isConnected()) {
        Serial.println("[WIFI] Connection lost");

        iot_task_ctx.sendTimer.stop();
        Serial.println("[TIMER] Send timer stopped");

        delete iot_task_wifi_manager;
        iot_task_wifi_manager = nullptr;
        
        if (iot_task_mqtt_service) {
            delete iot_task_mqtt_service;
            iot_task_mqtt_service = nullptr;
        }
        
        if (iot_task_tls_client){
            delete iot_task_tls_client;
            iot_task_tls_client = nullptr;
        }

        return SystemState::SYSTEM_STATE_WIFI_CONNECTING;
    }
    
    if (!iot_task_mqtt_service || !iot_task_mqtt_service->isConnected()) {
        if (!prv_initialize_mqtt_service()) {
            iot_task_ctx.mqttInitRetries++;
            
            if (iot_task_ctx.mqttInitRetries >= MAX_MQTT_INIT_RETRIES) {
                Serial.println("[MQTT] Max retries reached");
                iot_task_ctx.mqttInitRetries = 0;
                
                delete iot_task_wifi_manager;
                iot_task_wifi_manager = nullptr;
                
                if (iot_task_mqtt_service) { delete iot_task_mqtt_service; iot_task_mqtt_service = nullptr; }
                if (iot_task_tls_client) { delete iot_task_tls_client; iot_task_tls_client = nullptr; }
                
                return SystemState::SYSTEM_STATE_WIFI_CONNECTING;
            }
            
            Serial.printf("[MQTT] Init failed, retry %d/%d\n", iot_task_ctx.mqttInitRetries, MAX_MQTT_INIT_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
            return SystemState::SYSTEM_STATE_MQTT_OPERATING;
        }
        
        iot_task_ctx.mqttInitRetries = 0;
        Serial.println("[MQTT] Connected!");
        
        //avvio il timer perchè sono connesso a internet
        if (!iot_task_ctx.sendTimer.isRunning()) {
            iot_task_ctx.sendTimer.begin(15UL * 60UL * 1000UL);  // 15 minuti
            Serial.println("[TIMER] Send timer started");
        }
    }
    
    iot_task_mqtt_service->poll();
    
    //controllo la flag e pubblico telemetry
    if (iot_task_ctx.sendTimer.take()) {
        Serial.println("[TIMER] Time to publish telemetry");
        prv_publish_telemetry();
    }
    
    return SystemState::SYSTEM_STATE_MQTT_OPERATING;
}

static SystemState prv_handle_error() {
    Serial.println("[FSM] Error state - attempting recovery");
    vTaskDelay(pdMS_TO_TICKS(5000));
    return SystemState::SYSTEM_STATE_BOOT;
}

// ============================================================================
// TASK
// ============================================================================

static void prv_iot_task(void*) {
    // Initialize context
    esp_task_wdt_delete(NULL);
    memset(&iot_task_ctx, 0, sizeof(iot_task_ctx));
    iot_task_ctx.currentState = SystemState::SYSTEM_STATE_BOOT;
    iot_task_ctx.deviceId = 1;
    iot_task_ctx.testWifi = nullptr;
    iot_task_ctx.lastProgressSent = -1;
    
    iot_task_ctx.bleQueue = xQueueCreate(5, sizeof(BleMessage));
    if (!iot_task_ctx.bleQueue) {
        Serial.println("[FSM] Failed to create BLE queue");
        vTaskDelete(nullptr);
        return;
    }
    
    iot_task_ble_controller = new BleUartHal();
    iot_task_ble_controller->begin("PlantMonitor");
    iot_task_ble_controller->setRxHandler(prv_on_ble_data_received);
    
    Serial.println("[FSM] IoT Task started");
    Serial.printf("[FSM] Firmware: %s\n", FW_VERSION);
    
    while (true) {
        SystemState currentState = iot_task_ctx.currentState;
        SystemState nextState = currentState;
        
        switch (currentState) {
            case SystemState::SYSTEM_STATE_BOOT:             nextState = prv_handle_boot(); break;
            case SystemState::SYSTEM_STATE_BLE_ADVERTISING:  nextState = prv_handle_ble_advertising(); break;
            case SystemState::SYSTEM_STATE_BLE_CONFIGURING:  nextState = prv_handle_ble_configuring(); break;
            case SystemState::SYSTEM_STATE_BLE_TESTING_WIFI: nextState = prv_handle_ble_testing_wifi(); break;
            case SystemState::SYSTEM_STATE_WIFI_CONNECTING:  nextState = prv_handle_wifi_connecting(); break;
            case SystemState::SYSTEM_STATE_MQTT_OPERATING:   nextState = prv_handle_mqtt_operating(); break;
            case SystemState::SYSTEM_STATE_ERROR:      nextState = prv_handle_error(); break;
        }
        
        prv_log_state_transition(currentState, nextState);
        iot_task_ctx.currentState = nextState;
        
        vTaskDelay(pdMS_TO_TICKS(FSM_TICK_MS));
    }
}

void startIoTTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    xTaskCreatePinnedToCore(prv_iot_task, "IoTTask", stackSize, nullptr, priority, nullptr, core);
}

} // namespace Tasks
} // namespace PlantMonitor