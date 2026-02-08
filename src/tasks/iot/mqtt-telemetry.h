#pragma once

/*!
 * \file mqtt-telemetry.h
 * \brief MQTT telemetry publisher for IoT task
 *
 * Manages MQTT connection lifecycle and telemetry publishing.
 * Handles TLS setup, topic generation, and JSON payload creation.
 */

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "iot-task-types.h"

namespace PlantMonitor {
namespace IoT {
class MqttService;
}

namespace Tasks {

/*!
 * \class MqttTelemetryPublisher
 * \brief Manages MQTT telemetry publishing for the IoT task
 *
 * Responsibilities:
 * - Initialize MQTT service with TLS
 * - Generate device-specific topics
 * - Create telemetry JSON payloads
 * - Publish sensor data to broker
 */
class MqttTelemetryPublisher {
  public:
    /*!
     * \brief Constructor
     * \param broker MQTT broker address
     * \param port MQTT broker port
     * \param user MQTT username
     * \param password MQTT password
     * \param caCert Root CA certificate for TLS
     */
    MqttTelemetryPublisher(const char *broker, uint16_t port, const char *user, const char *password, const char *caCert);

    /*!
     * \brief Destructor - cleans up resources
     */
    ~MqttTelemetryPublisher();

    /*!
     * \brief Initialize MQTT service with TLS verification
     * \return true if initialization succeeded
     * \note Performs TLS handshake test before connecting
     */
    bool initialize();

    /*!
     * \brief Check if MQTT is connected
     * \return true if connected to broker
     */
    bool isConnected() const;

    /*!
     * \brief Poll MQTT client (must be called regularly)
     */
    void poll();

    /*!
     * \brief Disconnect and clean up resources
     */
    void disconnect();

    /*!
     * \brief Publish telemetry data
     * \param deviceId Device identifier for topic
     * \param data Sensor readings to publish
     * \return true if publish succeeded
     */
    bool publishTelemetry(int deviceId, const SensorData &data);

    /*!
     * \brief Generate MQTT topic for a device
     * \param deviceId Device identifier
     * \return Topic string (e.g., "plantform/esp32_001/telemetry")
     */
    static String generateDeviceTopic(int deviceId);

    /*!
     * \brief Create telemetry JSON payload
     * \param status Status string
     * \param data Sensor readings
     * \param deviceId Device identifier
     * \return JSON string payload
     */
    static String createTelemetryJson(const char *status,
                                      const SensorData &data,
                                      int deviceId);

  private:
    const char *m_broker;
    uint16_t m_port;
    const char *m_user;
    const char *m_password;
    const char *m_caCert;

    WiFiClientSecure *m_tlsClient;
    IoT::MqttService *m_mqttService;

    // Prevent copying
    MqttTelemetryPublisher(const MqttTelemetryPublisher &) = delete;
    MqttTelemetryPublisher &operator=(const MqttTelemetryPublisher &) = delete;
};

} // namespace Tasks
} // namespace PlantMonitor
