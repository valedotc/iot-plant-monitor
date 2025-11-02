#pragma once

#include <Arduino.h>
#include <ArduinoMqttClient.h>
#include <WiFiS3.h>

/*!
 * \file mqtt_service.h
 * \brief MQTT communication service
 * 
 * This class handles MQTT protocol operations including publish, subscribe,
 * and message handling.
 * 
 * \note Thread-safe when used with proper mutex
 */

namespace PlantMonitor {
namespace IoT {

/*!
 * \brief Callback function type for message reception
 * \param topic The topic on which the message was received
 * \param payload The message payload as a String
 */
typedef void (*MqttMessageCallback)(const char* topic, const String& payload);

/*!
 * \class MqttService
 * \brief Service for MQTT protocol communication
 * 
 * Manages MQTT broker connection, publish/subscribe operations, and message handling.
 */
class MqttService {
  public:
    /*!
     * \brief Constructor
     * \param wifi_client Pointer to WiFiSSLClient for secure connection
     * \param broker MQTT broker address
     * \param port MQTT broker port
     * \param username MQTT username
     * \param password MQTT password
     */
    MqttService(WiFiSSLClient* wifi_client, const char* broker, int port,
                const char* username, const char* password);

    /*!
     * \brief Destructor
     */
    ~MqttService();

    /*!
     * \brief Initialize and connect to MQTT broker
     * \return true if connection successful, false otherwise
     */
    bool begin();

    /*!
     * \brief Disconnect from MQTT broker
     */
    void disconnect();

    /*!
     * \brief Check if MQTT client is connected
     * \return true if connected, false otherwise
     */
    bool isConnected() const;

    /*!
     * \brief Publish a message to a topic
     * \param topic MQTT topic
     * \param message Message payload
     * \param retain Retain flag for persistent messages
     * \return true if publish successful, false otherwise
     */
    bool publish(const char* topic, const char* message, bool retain = false);

    /*!
     * \brief Subscribe to a topic
     * \param topic MQTT topic to subscribe to
     * \return true if subscription successful, false otherwise
     */
    bool subscribe(const char* topic);

    /*!
     * \brief Unsubscribe from a topic
     * \param topic MQTT topic to unsubscribe from
     * \return true if unsubscription successful, false otherwise
     */
    bool unsubscribe(const char* topic);

    /*!
     * \brief Poll for incoming messages (must be called regularly in loop)
     */
    void poll();

    /*!
     * \brief Set callback function for incoming messages
     * \param callback Function to call when a message is received
     */
    void setMessageCallback(MqttMessageCallback callback);

  private:
    WiFiSSLClient* m_wifi_client;  //!< WiFi SSL client pointer
    MqttClient* m_mqtt_client;     //!< MQTT client pointer
    const char* m_broker;          //!< MQTT broker address
    int m_port;                    //!< MQTT broker port
    const char* m_username;        //!< MQTT username
    const char* m_password;        //!< MQTT password
    MqttMessageCallback m_callback; //!< Message callback function

    /*!
     * \brief Internal message handler
     * \param message_size Size of the received message
     */
    static void onMessageReceived(int message_size);

    /*!
     * \brief Static instance pointer for callback
     */
    static MqttService* s_instance;

    // Prevent copying
    MqttService(const MqttService&) = delete;
    MqttService& operator=(const MqttService&) = delete;
};

} // namespace IoT
} // namespace PlantMonitor