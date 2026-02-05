#include "mqtt-service.h"

namespace PlantMonitor {
namespace IoT {

MqttService *MqttService::s_instance = nullptr;
SemaphoreHandle_t MqttService::s_instance_mutex = nullptr;

MqttService::MqttService(WiFiClientSecure *wifi_client,
                         const char *broker,
                         int port,
                         const char *username,
                         const char *password)
    : m_wifi_client(wifi_client),
      m_broker(broker),
      m_port(port),
      m_username(username),
      m_password(password),
      m_callback(nullptr) {

    if (s_instance_mutex == nullptr) {
        s_instance_mutex = xSemaphoreCreateMutex();
    }
    
    m_mutex = xSemaphoreCreateMutex();
    
    if (s_instance_mutex != nullptr && xSemaphoreTake(s_instance_mutex, portMAX_DELAY) == pdTRUE) {
        s_instance = this;
        xSemaphoreGive(s_instance_mutex);
    }

    m_mqtt_client = std::make_unique<MqttClient>(*m_wifi_client);
}

MqttService::~MqttService() {
    if (xSemaphoreTake(s_instance_mutex, portMAX_DELAY) == pdTRUE) {
        s_instance = nullptr;
        xSemaphoreGive(s_instance_mutex);
    }

    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
    }
}

bool MqttService::begin() {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE)
        return false;

    m_mqtt_client.reset();
    m_mqtt_client = std::make_unique<MqttClient>(*m_wifi_client);

    m_mqtt_client->setUsernamePassword(m_username, m_password);
    m_mqtt_client->setId("esp32_001"); 

    Serial.printf("[MQTT] Connecting to %s:%d\n", m_broker, m_port);

    bool success = m_mqtt_client->connect(m_broker, m_port);
    if (!success) {
        Serial.printf("[MQTT] Connection failed! Code: %d\n", m_mqtt_client->connectError());
        Serial.printf("[MQTT] TLS socket connected(): %d\n", m_wifi_client->connected());
        xSemaphoreGive(m_mutex);
        return false;
    }

    Serial.println("[MQTT] Connected successfully!");

    m_mqtt_client->onMessage([](int size) {
        if (xSemaphoreTake(s_instance_mutex, portMAX_DELAY) == pdTRUE) {
            if (s_instance)
                s_instance->onMessageReceived(size);
            xSemaphoreGive(s_instance_mutex);
        }
    });

    xSemaphoreGive(m_mutex);
    return true;
}


void MqttService::disconnect() {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        m_mqtt_client->stop();
        xSemaphoreGive(m_mutex);
    }
}

bool MqttService::isConnected() const {
    bool result = false;
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        result = m_mqtt_client->connected();
        xSemaphoreGive(m_mutex);
    }
    return result;
}

bool MqttService::publish(const char *topic, const char *message, bool retain) {
    bool result = false;
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        m_mqtt_client->beginMessage(topic, retain);
        m_mqtt_client->print(message);
        result = (m_mqtt_client->endMessage() == 1);
        xSemaphoreGive(m_mutex);
    }
    return result;
}

bool MqttService::subscribe(const char *topic) {
    bool result = false;
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        result = (m_mqtt_client->subscribe(topic) == 1);
        xSemaphoreGive(m_mutex);
    }
    return result;
}

bool MqttService::unsubscribe(const char *topic) {
    bool result = false;
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        result = (m_mqtt_client->unsubscribe(topic) == 1);
        xSemaphoreGive(m_mutex);
    }
    return result;
}

void MqttService::poll() {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        m_mqtt_client->poll();
        xSemaphoreGive(m_mutex);
    }
}

void MqttService::setMessageCallback(MqttMessageCallback callback) {
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) == pdTRUE) {
        m_callback = callback;
        xSemaphoreGive(m_mutex);
    }
}

void MqttService::onMessageReceived(int message_size) {
    if (xSemaphoreTake(s_instance->m_mutex, portMAX_DELAY) == pdTRUE) {
        if (!s_instance->m_callback) {
            xSemaphoreGive(s_instance->m_mutex);
            return;
        }

        String topic = s_instance->m_mqtt_client->messageTopic();
        String payload;
        payload.reserve(message_size);

        while (s_instance->m_mqtt_client->available()) {
            payload += static_cast<char>(s_instance->m_mqtt_client->read());
        }

        s_instance->m_callback(topic, payload);
        xSemaphoreGive(s_instance->m_mutex);
    }
}


} // namespace IoT
} // namespace PlantMonitor
