#include "mqtt_service.h"

namespace PlantMonitor {
namespace IoT {

// Static instance for callback
MqttService* MqttService::s_instance = nullptr;

MqttService::MqttService(WiFiSSLClient* wifi_client, const char* broker, int port,
                         const char* username, const char* password)
    : m_wifi_client(wifi_client)
    , m_broker(broker)
    , m_port(port)
    , m_username(username)
    , m_password(password)
    , m_callback(nullptr) {
    
    m_mqtt_client = new MqttClient(*m_wifi_client);
    s_instance = this;
}

MqttService::~MqttService() {
    if (m_mqtt_client) {
        delete m_mqtt_client;
        m_mqtt_client = nullptr;
    }
    s_instance = nullptr;
}

bool MqttService::begin() {
    m_mqtt_client->setUsernamePassword(m_username, m_password);
    
    Serial.print("[MQTT] Connecting to broker: ");
    Serial.print(m_broker);
    Serial.print(":");
    Serial.println(m_port);
    
    if (!m_mqtt_client->connect(m_broker, m_port)) {
        Serial.print("[MQTT] Connection failed! Error code: ");
        Serial.println(m_mqtt_client->connectError());
        return false;
    }
    
    Serial.println("[MQTT] Connected successfully!");
    
    // Set up message callback
    m_mqtt_client->onMessage(onMessageReceived);
    
    return true;
}

void MqttService::disconnect() {
    Serial.println("[MQTT] Disconnecting...");
    m_mqtt_client->stop();
}

bool MqttService::isConnected() const {
    return m_mqtt_client->connected();
}

bool MqttService::publish(const char* topic, const char* message, bool retain) {
    Serial.print("[MQTT] Publishing to '");
    Serial.print(topic);
    Serial.print("': ");
    Serial.println(message);
    
    m_mqtt_client->beginMessage(topic, retain);
    m_mqtt_client->print(message);
    int result = m_mqtt_client->endMessage();
    
    if (result == 0) {
        Serial.println("[MQTT] Message published successfully");
        return true;
    } else {
        Serial.print("[MQTT] Publish failed with error: ");
        Serial.println(result);
        return false;
    }
}

bool MqttService::subscribe(const char* topic) {
    Serial.print("[MQTT] Subscribing to topic: ");
    Serial.println(topic);
    
    int result = m_mqtt_client->subscribe(topic);
    if (result == 1) {
        Serial.println("[MQTT] Subscription successful");
        return true;
    } else {
        Serial.println("[MQTT] Subscription failed");
        return false;
    }
}

bool MqttService::unsubscribe(const char* topic) {
    Serial.print("[MQTT] Unsubscribing from topic: ");
    Serial.println(topic);
    
    int result = m_mqtt_client->unsubscribe(topic);
    if (result == 1) {
        Serial.println("[MQTT] Unsubscription successful");
        return true;
    } else {
        Serial.println("[MQTT] Unsubscription failed");
        return false;
    }
}

void MqttService::poll() {
    m_mqtt_client->poll();
}

void MqttService::setMessageCallback(MqttMessageCallback callback) {
    m_callback = callback;
}

void MqttService::onMessageReceived(int message_size) {
    if (!s_instance || !s_instance->m_callback) {
        return;
    }
    
    String topic = s_instance->m_mqtt_client->messageTopic();
    String payload = "";
    
    while (s_instance->m_mqtt_client->available()) {
        payload += (char)s_instance->m_mqtt_client->read();
    }
    
    Serial.print("[MQTT] Message received on '");
    Serial.print(topic);
    Serial.print("': ");
    Serial.println(payload);
    
    s_instance->m_callback(topic.c_str(), payload);
}

} // namespace IoT
} // namespace PlantMonitor