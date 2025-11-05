# PlantMonitor IoT - MQTT Service

## Overview

The **`MqttService`** class simplifies MQTT communication for Arduino-based IoT devices.  
It handles connecting to an MQTT broker, publishing and subscribing to topics, and receiving messages securely over Wi-Fi using SSL/TLS.

---

## Requirements

Make sure you have the following libraries installed:

- [`WiFiS3`](https://www.arduino.cc/reference/en/libraries/wifis3/)
- [`ArduinoMqttClient`](https://www.arduino.cc/reference/en/libraries/arduinomqttclient/)

---

## Configuration

Copy the provided `network_config.example.h` file to `network_config.h` and fill in your credentials:

```cpp
namespace Config {
constexpr char WIFI_SSID[] = "YourSSID";
constexpr char WIFI_PASSWORD[] = "YourPassword";

constexpr char MQTT_BROKER[] = "your-broker-url.hivemq.cloud";
constexpr int MQTT_PORT = 8883;
constexpr char MQTT_USER[] = "your-username";
constexpr char MQTT_PASSWORD[] = "your-password";

constexpr char MQTT_TOPIC_STATUS[] = "plant_monitor/status";
constexpr char MQTT_TOPIC_COMMANDS[] = "plant_monitor/commands";
} // namespace Config

```

---

## Example Usage

```cpp

#include <WiFiS3.h>
#include "mqtt_service.h"
#include "network_config.h"

using namespace PlantMonitor::IoT;

WiFiSSLClient wifi_client;
MqttService mqtt(&wifi_client,
                 Config::MQTT_BROKER,
                 Config::MQTT_PORT,
                 Config::MQTT_USER,
                 Config::MQTT_PASSWORD);

void onMessage(String topic, String payload) {
    Serial.print("Received on ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(payload);
}

void setup() {
    Serial.begin(115200);

    // Connect to Wi-Fi
    WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Wi-Fi connected!");

    // Connect to MQTT
    mqtt.setMessageCallback(onMessage);
    if (mqtt.begin()) {
        mqtt.subscribe(Config::MQTT_TOPIC_COMMANDS);
        mqtt.publish(Config::MQTT_TOPIC_STATUS, "Device online");
    }
}

void loop() {
    mqtt.poll(); // must be called regularly
}

```

## Basic APIs

| Method                            | Description                              |
| --------------------------------- | ---------------------------------------- |
| `begin()`                         | Connect to MQTT broker                   |
| `disconnect()`                    | Disconnect from broker                   |
| `isConnected()`                   | Check connection status                  |
| `publish(topic, message, retain)` | Publish a message (`retain` is a boolean: if true, the broker retains the message) |
| `subscribe(topic)`                | Subscribe to a topic                     |
| `unsubscribe(topic)`              | Unsubscribe from a topic                 |
| `setMessageCallback(callback)`    | Register message callback                |
| `poll()`                          | Must be called regularly in the main loop to process incoming MQTT messages and maintain connection. |
