
#include <Arduino.h>
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

/*! \defgroup Timing Timing Configurations
 *  @{
 */
#define UI_UPDATE_INTERVAL_MS 500    /*!< UI update interval */
#define MQTT_PUB_INTERVAL_MS 5000    /*!< Telemetry publish interval */
#define RECONNECT_DELAY_MS 1000      /*!< Delay between reconnection attempts */
/*! @} */

#define OPERA_SENZA_TITOLO_FRAME_COUNT 1
#define OPERA_SENZA_TITOLO_FRAME_WIDTH 128
#define OPERA_SENZA_TITOLO_FRAME_HEIGHT 128


using namespace PlantMonitor::Drivers;
using namespace PlantMonitor::IoT;

// ============================================================================
// TYPE AND STRUCTURE DECLARATIONS
// ============================================================================

/*!
 * \brief System finite state machine states
 */
enum class SystemState {
    BOOT,              /*!< System initialization */
    BLE_ADVERTISING,   /*!< BLE advertising for configuration */
    BLE_CONFIGURING,   /*!< Receiving configuration via BLE */
    WIFI_CONNECTING,   /*!< WiFi connection in progress */
    MQTT_OPERATING     /*!< Normal operations with MQTT */
};

/*!
 * \brief Sensor telemetry data structure
 */
struct SensorData {
    float temperature;  /*!< Temperature in degrees Celsius */
    float humidity;     /*!< Relative humidity in percentage */
    float moisture;     /*!< Soil moisture level */
    bool lightDetected; /*!< Light presence detected */
};

/*!
 * \brief Global system context
 */
struct SystemContext {
    SystemState currentState;           /*!< Current FSM state */
    int deviceId;                       /*!< Device unique ID */
    unsigned long lastUiUpdate;         /*!< Last UI update timestamp */
    unsigned long lastMqttPublish;      /*!< Last MQTT publish timestamp */
    SensorData sensorReadings;          /*!< Current sensor readings */
    String bleReceivedMessage;          /*!< Received BLE message buffer */
    volatile bool bleMessageReady;      /*!< BLE message ready flag */
};

// ============================================================================
// HARDWARE AND SERVICES INSTANCES
// ============================================================================

/*! \defgroup HardwareDrivers Hardware Drivers
 *  @{
 */
static DisplayHAL* displayDriver = nullptr;           /*!< OLED display driver */
static bme280HAL* environmentalSensor = nullptr;      /*!< Temperature/humidity sensor */
static MoistureSensorHAL* moistureSensor = nullptr;   /*!< Soil moisture sensor */
static BleUartHal* bleController = nullptr;           /*!< Bluetooth LE controller */
static WiFiHal* wifiManager = nullptr;                /*!< WiFi connection manager */
/*! @} */

/*! \defgroup NetworkServices Network Services
 *  @{
 */
static WiFiClientSecure* tlsClient = nullptr;         /*!< TLS client for MQTT */
static MqttService* mqttService = nullptr;            /*!< MQTT service */
/*! @} */

/*! \brief Global system context */
static SystemContext systemCtx;

// ============================================================================
// MQTT AND TELEMETRY FUNCTIONS
// ============================================================================

/*!
 * \brief Generates the MQTT topic for device telemetry
 * \param deviceID Device unique ID
 * \return String containing the topic in format "devices/{ID}/telemetry"
 */
String generateDeviceTopic(int deviceID) {
    return "plantform/esp32_00" + String(deviceID, 3) + "/telemetry";
}

/*!
 * \brief Creates JSON payload for telemetry
 * \param status Device descriptive status
 * \param temperature Detected temperature (°C)
 * \param humidity Relative humidity (%)
 * \param moisture Soil moisture level
 * \param light Light presence
 * \return Formatted JSON string
 */
String createTelemetryJson(const String& status, float temperature, 
                          float humidity, float moisture, bool light) {
    String json;
    json.reserve(256);
    
    json = "{\n";
    json += "  \"status\": \"" + status + "\",\n";
    json += "  \"temperature\": " + String(temperature, 2) + ",\n";
    json += "  \"humidity\": " + String(humidity, 2) + ",\n";
    json += "  \"moisture\": " + String(moisture, 2) + ",\n";
    json += "  \"light\": " + String(light ? "true" : "false") + "\n";
    json += "}";
    
    return json;
}

// ============================================================================
// SENSOR MANAGEMENT FUNCTIONS
// ============================================================================

/*!
 * \brief Reads all sensors and updates the context
 * \param[out] data Structure to populate with readings
 * \return true if all readings are valid, false otherwise
 */
bool readAllSensors(SensorData& data) {
    if (!environmentalSensor || !moistureSensor) {
        Serial.println("[SENSORS] Drivers not initialized");
        return false;
    }
    
    data.temperature = environmentalSensor->readTemperature();
    data.humidity = environmentalSensor->readHumidity();
    data.moisture = moistureSensor->readMoistureLevel();
    data.lightDetected = true; // TODO: implement real light sensor
    
    // Reading validation
    if (isnan(data.temperature) || isnan(data.humidity)) {
        Serial.println("[SENSORS] Invalid readings detected");
        return false;
    }
    
    return true;
}

// ============================================================================
// UI MANAGEMENT FUNCTIONS
// ============================================================================

/*!
 * \brief Updates the display with current state information
 * \param state Current system state
 * \param message Message to display (optional)
 */
void updateDisplay(SystemState state, const String& message = "") {
    if (!displayDriver) return;
    
    displayDriver->clear();
    displayDriver->setCursor(0, 0);
    
    switch (state) {
        case SystemState::BOOT:
            displayDriver->printf("=== BOOT ===\n");
            displayDriver->printf("Initializing...\n");
            break;
            
        case SystemState::BLE_ADVERTISING:
            displayDriver->printf("=== BLE ===\n");
            displayDriver->printf("Advertising active\n");
            displayDriver->printf("Waiting connection...\n");
            break;
            
        case SystemState::BLE_CONFIGURING:
            displayDriver->printf("=== BLE ===\n");
            displayDriver->printf("CONNECTED!\n");
            displayDriver->printf("Waiting config...\n");
            break;
            
        case SystemState::WIFI_CONNECTING:
            displayDriver->printf("=== WiFi ===\n");
            displayDriver->printf("Connecting...\n");
            break;
            
        case SystemState::MQTT_OPERATING:
            displayDriver->printf("=== OPERATIONAL ===\n");
            displayDriver->printf("MQTT: %s\n", 
                mqttService && mqttService->isConnected() ? "OK" : "NO");
            displayDriver->printf("T: %.1f C\n", systemCtx.sensorReadings.temperature);
            displayDriver->printf("H: %.1f %%\n", systemCtx.sensorReadings.humidity);
            displayDriver->printf("M: %.1f\n", systemCtx.sensorReadings.moisture);
            break;
    }
    
    if (message.length() > 0) {
        displayDriver->printf("\n%s\n", message.c_str());
    }
    
    displayDriver->update();
}

// ============================================================================
// FSM STATE HANDLERS
// ============================================================================

/*!
 * \brief Handles the BOOT state
 * \return Next system state
 */
SystemState handleBootState() {
    Serial.println("\n[FSM] BOOT State");
    
    if (!ConfigHandler::isConfigured()) {
        Serial.println("[BOOT] No configuration found");
        updateDisplay(SystemState::BLE_ADVERTISING);
        
        if (bleController->startAdvertising_()) {
            return SystemState::BLE_ADVERTISING;
        } else {
            Serial.println("[BOOT] Error starting advertising");
            return SystemState::BOOT;
        }
    } else {
        Serial.println("[BOOT] Existing configuration found");
        updateDisplay(SystemState::WIFI_CONNECTING);
        return SystemState::WIFI_CONNECTING;
    }
}

/*!
 * \brief Handles the BLE_ADVERTISING state
 * \return Next system state
 */
SystemState handleBleAdvertisingState() {
    // Periodic UI update
    if (millis() - systemCtx.lastUiUpdate > UI_UPDATE_INTERVAL_MS) {
        updateDisplay(SystemState::BLE_ADVERTISING);
        systemCtx.lastUiUpdate = millis();
    }
    
    // Check BLE connection
    if (bleController->isConnected()) {
        Serial.println("[BLE] Client connected");
        return SystemState::BLE_CONFIGURING;
    }
    
    return SystemState::BLE_ADVERTISING;
}

/*!
 * \brief Handles the BLE_CONFIGURING state
 * \return Next system state
 */
SystemState handleBleConfiguringState() {
    // Check disconnection
    if (!bleController->isConnected()) {
        Serial.println("[BLE] Client disconnected during configuration");
        return SystemState::BLE_ADVERTISING;
    }
    
    // Periodic UI update
    if (millis() - systemCtx.lastUiUpdate > UI_UPDATE_INTERVAL_MS) {
        updateDisplay(SystemState::BLE_CONFIGURING);
        systemCtx.lastUiUpdate = millis();
    }
    
    // Process received message
    if (systemCtx.bleMessageReady) {
        systemCtx.bleMessageReady = false;
        
        AppConfig config;
        if (!ConfigHandler::parseAppCfg(systemCtx.bleReceivedMessage.c_str(), config)) {
            Serial.println("[BLE] Configuration parsing error");
            updateDisplay(SystemState::BLE_CONFIGURING, "FORMAT ERROR!");
            delay(2000);
            return SystemState::BLE_ADVERTISING;
        }
        
        // Save configuration
        if (ConfigHandler::save(config)) {
            Serial.println("[BLE] Configuration saved successfully");
            return SystemState::WIFI_CONNECTING;
        } else {
            Serial.println("[BLE] Configuration save error");
            updateDisplay(SystemState::BLE_CONFIGURING, "SAVE ERROR!");
            delay(2000);
            return SystemState::BLE_ADVERTISING;
        }
    }
    
    return SystemState::BLE_CONFIGURING;
}

/*!
 * \brief Handles the WIFI_CONNECTING state
 * \return Next system state
 */
SystemState handleWifiConnectingState() {
    if (!ConfigHandler::isConfigured()) {
        Serial.println("[WiFi] Configuration lost, returning to BLE");
        return SystemState::BLE_ADVERTISING;
    }
    
    AppConfig config;
    ConfigHandler::load(config);
    
    // Initialize WiFi manager if needed
    if (!wifiManager) {
        wifiManager = new WiFiHal(config.ssid.c_str(), config.password.c_str());
        Serial.printf("[WiFi] Connection attempt to: %s\n", config.ssid.c_str());
    }
    
    // Connection attempt
    if (!wifiManager->isConnected()) {
        wifiManager->begin();
        updateDisplay(SystemState::WIFI_CONNECTING, "Connecting...");
        delay(500);
    }
    
    // Verify successful connection
    if (wifiManager->isConnected()) {
        Serial.println("[WiFi] Successfully connected");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());
        updateDisplay(SystemState::WIFI_CONNECTING, "CONNECTED!");
        delay(1000);
        return SystemState::MQTT_OPERATING;
    }
    
    return SystemState::WIFI_CONNECTING;
}

/*!
 * \brief Initializes the MQTT service
 * \return true if initialization successful, false otherwise
 */
bool initializeMqttService() {
    if (mqttService) {
        return true; // Already initialized
    }
    
    // Create TLS client
    tlsClient = new WiFiClientSecure();
    tlsClient->setCACert(HIVEMQ_ROOT_CA);
    
    // Test TLS connection
    Serial.println("[MQTT] Testing TLS connection...");
    if (tlsClient->connect(MQTT_BROKER, MQTT_PORT)) {
        Serial.println("[MQTT] TLS test successful");
        tlsClient->stop();
    } else {
        Serial.println("[MQTT] TLS test failed");
        return false;
    }
    
    // Create MQTT service
    mqttService = new MqttService(tlsClient, MQTT_BROKER, MQTT_PORT, 
                                  MQTT_USER, MQTT_PASSWORD);
    
    // Register message callback
    mqttService->setMessageCallback([](String topic, String payload) {
        Serial.printf("[MQTT] RX - Topic: %s\n", topic.c_str());
        Serial.printf("[MQTT] RX - Payload: %s\n", payload.c_str());
    });
    
    // Start service
    if (!mqttService->begin()) {
        Serial.println("[MQTT] Service initialization error");
        delete mqttService;
        mqttService = nullptr;
        return false;
    }
    
    Serial.println("[MQTT] Service initialized successfully");
    return true;
}

/*!
 * \brief Publishes telemetry on MQTT
 * \return true if publish successful, false otherwise
 */
bool publishTelemetry() {
    if (!mqttService || !mqttService->isConnected()) {
        return false;
    }
    
    // Read sensors
    if (!readAllSensors(systemCtx.sensorReadings)) {
        Serial.println("[TELEMETRY] Sensor reading error");
        return false;
    }
    
    // Create message
    String message = createTelemetryJson(
        "staus",
        systemCtx.sensorReadings.temperature,
        systemCtx.sensorReadings.humidity,
        systemCtx.sensorReadings.moisture,
        systemCtx.sensorReadings.lightDetected
    );
    
    String topic = generateDeviceTopic(systemCtx.deviceId);
    
    Serial.printf("[TELEMETRY] Publishing to %s\n", topic.c_str());
    Serial.println(message);
    
    // Publish
    bool success = mqttService->publish(topic.c_str(), message.c_str(), false);
    Serial.printf("[TELEMETRY] Result: %s\n", success ? "OK" : "FAIL");
    
    return success;
}

/*!
 * \brief Handles the MQTT_OPERATING state
 * \return Next system state
 */
SystemState handleMqttOperatingState() {
    // Initialize MQTT service if needed
    if (!mqttService && !initializeMqttService()) {
        updateDisplay(SystemState::MQTT_OPERATING, "MQTT INIT FAIL");
        delay(RECONNECT_DELAY_MS);
        return SystemState::MQTT_OPERATING;
    }
    
    // Attempt connection if disconnected
    if (!mqttService->isConnected()) {
        updateDisplay(SystemState::MQTT_OPERATING, "MQTT Reconnecting...");
        delay(RECONNECT_DELAY_MS);

        if(!wifiManager->isConnected()) {
            Serial.println("[MQTT] WiFi disconnected, returning to WiFi state");
            return SystemState::WIFI_CONNECTING;
        }
        mqttService->begin();
        return SystemState::MQTT_OPERATING;
    }
    
    // MQTT polling
    mqttService->poll();
    
    // Periodic telemetry publishing
    if (millis() - systemCtx.lastMqttPublish >= MQTT_PUB_INTERVAL_MS) {
        systemCtx.lastMqttPublish = millis();
        publishTelemetry();
        updateDisplay(SystemState::MQTT_OPERATING);
    }
    
    return SystemState::MQTT_OPERATING;
}

// ============================================================================
// SETUP AND MAIN LOOP
// ============================================================================

/*!
 * \brief Callback for BLE data reception
 * \param data Bytes received via BLE UART
 */
void onBleDataReceived(const BleUartHal::Bytes& data) {
    systemCtx.bleReceivedMessage = "";
    systemCtx.bleReceivedMessage.reserve(data.size());
    
    for (auto byte : data) {
        systemCtx.bleReceivedMessage += (char)byte;
    }
    
    systemCtx.bleMessageReady = true;
    Serial.printf("[BLE] Message received: %s\n", 
                  systemCtx.bleReceivedMessage.c_str());
}

/*!
 * \brief System initialization
 */
void setup() {
    // Initialize context
    systemCtx.currentState = SystemState::BOOT;
    systemCtx.deviceId = 1;
    systemCtx.lastUiUpdate = 0;
    systemCtx.lastMqttPublish = 0;
    systemCtx.bleMessageReady = false;
    
    // Initialize serial
    Serial.begin(115200);
    delay(100);
    Serial.println("\n");
    Serial.println("=====================================");
    Serial.println("  Plant Monitor System v2.0");
    Serial.println("  ESP32 IoT Monitoring System");
    Serial.println("=====================================");
    
    // Initialize display
    displayDriver = new DisplayHAL();
    if (!displayDriver->begin()) {
        Serial.println("[FATAL] Cannot initialize display!");
        while (1) delay(1000);
    }
    displayDriver->setTextSize(1);
    displayDriver->setTextColor(COLOR_WHITE);
    Serial.println("[INIT] Display initialized");
    
    // Initialize sensors
    environmentalSensor = new bme280HAL();
    if (!environmentalSensor->begin()) {
        Serial.println("[ERROR] BME280 initialization error");
    } else {
        Serial.println("[INIT] BME280 initialized");
    }
    
    moistureSensor = new MoistureSensorHAL();
    if (!moistureSensor->begin()) {
        Serial.println("[ERROR] Moisture sensor initialization error");
    } else {
        Serial.println("[INIT] Moisture sensor initialized");
    }
    
    // Initialize BLE
    bleController = new BleUartHal();
    if (!bleController->begin("PlantMonitor")) {
        Serial.println("[FATAL] Cannot initialize BLE!");
        updateDisplay(SystemState::BOOT, "BLE INIT FAIL\nREBOOT NEEDED");
        while (1) delay(1000);
    }
    bleController->setRxHandler(onBleDataReceived);
    Serial.println("[INIT] BLE initialized");
    
    // NOTE: Uncomment to reset configuration during development
    //ConfigHandler::clear();
    //Serial.println("[DEV] Configuration cleared");
    
    Serial.println("[INIT] System ready\n");
}

/*!
 * \brief Main loop - executes the FSM
 */
void loop() {
    SystemState nextState;
    
    switch (systemCtx.currentState) {
        case SystemState::BOOT:
            nextState = handleBootState();
            break;
            
        case SystemState::BLE_ADVERTISING:
            nextState = handleBleAdvertisingState();
            break;
            
        case SystemState::BLE_CONFIGURING:
            nextState = handleBleConfiguringState();
            break;
            
        case SystemState::WIFI_CONNECTING:
            nextState = handleWifiConnectingState();
            break;
            
        case SystemState::MQTT_OPERATING:
            nextState = handleMqttOperatingState();
            break;
            
        default:
            Serial.println("[ERROR] Unknown state detected!");
            updateDisplay(SystemState::BOOT, "STATE ERROR!");
            nextState = SystemState::BOOT;
            delay(2000);
            break;
    }
    
    // State transition
    if (nextState != systemCtx.currentState) {
        Serial.printf("[FSM] Transition: %d -> %d\n", 
                     (int)systemCtx.currentState, (int)nextState);
        systemCtx.currentState = nextState;
    }
    
    // Small delay to avoid watchdog
    delay(10);
}