/*!
 * \file mqtt-telemetry.cpp
 * \brief Implementation of MQTT telemetry publisher
 */

#include "mqtt-telemetry.h"
#include "iot/mqtt-service.h"
#include <esp_task_wdt.h>

using namespace PlantMonitor::IoT;

namespace PlantMonitor {
namespace Tasks {

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

MqttTelemetryPublisher::MqttTelemetryPublisher(
    const char *broker, uint16_t port, const char *user, const char *password, const char *caCert)
    : m_broker(broker), m_port(port), m_user(user), m_password(password), m_caCert(caCert), m_tlsClient(nullptr), m_mqttService(nullptr) {
}

MqttTelemetryPublisher::~MqttTelemetryPublisher() {
    disconnect();
}

// ============================================================================
// CONNECTION MANAGEMENT
// ============================================================================

bool MqttTelemetryPublisher::initialize() {
    if (m_mqttService && m_mqttService->isConnected()) {
        return true;
    }

    // Create TLS client if needed
    if (!m_tlsClient) {
        m_tlsClient = new WiFiClientSecure();
        m_tlsClient->setCACert(m_caCert);

        Serial.println("[MQTT] Testing TLS connection...");

        // Avoid triggering watchdog during TLS handshake
        esp_task_wdt_reset();

        if (!m_tlsClient->connect(m_broker, m_port)) {
            Serial.println("[MQTT] TLS test failed");
            delete m_tlsClient;
            m_tlsClient = nullptr;
            return false;
        }
        m_tlsClient->stop();
        Serial.println("[MQTT] TLS test OK");
    }

    // Create MQTT service if needed
    if (!m_mqttService) {
        m_mqttService = new MqttService(m_tlsClient, m_broker, m_port, m_user, m_password);
        m_mqttService->setMessageCallback([](String topic, String payload) {
            Serial.printf("[MQTT] RX %s: %s\n", topic.c_str(), payload.c_str());
        });
    }

    return m_mqttService->begin();
}

bool MqttTelemetryPublisher::isConnected() const {
    return m_mqttService && m_mqttService->isConnected();
}

void MqttTelemetryPublisher::poll() {
    if (m_mqttService) {
        m_mqttService->poll();
    }
}

void MqttTelemetryPublisher::disconnect() {
    if (m_mqttService) {
        delete m_mqttService;
        m_mqttService = nullptr;
    }
    if (m_tlsClient) {
        delete m_tlsClient;
        m_tlsClient = nullptr;
    }
}

// ============================================================================
// TELEMETRY PUBLISHING
// ============================================================================

bool MqttTelemetryPublisher::publishTelemetry(int deviceId, const SensorData &data) {
    if (!isConnected()) {
        return false;
    }

    String topic = generateDeviceTopic(deviceId);
    String payload = createTelemetryJson("ok", data, deviceId);

    bool success = m_mqttService->publish(topic.c_str(), payload.c_str(), false);
    if (success) {
        Serial.printf("[MQTT] Published to %s\n", topic.c_str());
    }

    return success;
}

// ============================================================================
// STATIC HELPERS
// ============================================================================

String MqttTelemetryPublisher::generateDeviceTopic(int deviceId) {
    char topic[64];
    snprintf(topic, sizeof(topic), "plantformio/esp32_%03d/telemetry", deviceId);
    return String(topic);
}

String MqttTelemetryPublisher::createTelemetryJson(
    const char *status, const SensorData &data, int deviceId) {

    char json[256];
    snprintf(json, sizeof(json), "{\"status\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,"
                                 "\"moisture\":%.2f,\"light\":%s,\"device_id\":%d}",
             status,
             data.temperature,
             data.humidity,
             data.moisture,
             data.lightDetected ? "true" : "false",
             deviceId);
    return String(json);
}

} // namespace Tasks
} // namespace PlantMonitor
