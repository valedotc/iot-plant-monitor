#include "wifi_hal.h"

namespace PlantMonitor {
namespace Drivers {

WiFiHal::WiFiHal(const char* ssid, const char* password,
                 int max_attempts, int retry_delay_ms)
    : m_ssid(ssid)
    , m_password(password)
    , m_max_attempts(max_attempts)
    , m_retry_delay_ms(retry_delay_ms) {
}

bool WiFiHal::begin() {
    WiFi.disconnect();
    delay(1000);
    
    Serial.println("[WiFi] Connecting to network...");
    WiFi.begin(m_ssid, m_password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < m_max_attempts) {
        delay(m_retry_delay_ms);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection failed!");
        return false;
    }
    
    Serial.println("[WiFi] Connected successfully!");
    
    if (!waitForValidIP()) {
        Serial.println("[WiFi] ERROR: Failed to obtain valid IP address");
        Serial.println("[WiFi] Troubleshooting:");
        Serial.println("[WiFi]   1. Restart the router");
        Serial.println("[WiFi]   2. Verify DHCP is enabled");
        Serial.println("[WiFi]   3. Reload the Arduino sketch");
        return false;
    }
    
    printStatus();
    return true;
}

bool WiFiHal::waitForValidIP() {
    Serial.print("[WiFi] Waiting for IP address");
    
    int attempts = 0;
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0) && attempts < 10) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    Serial.println();
    
    return WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

void WiFiHal::disconnect() {
    Serial.println("[WiFi] Disconnecting...");
    WiFi.disconnect();
}

bool WiFiHal::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

IPAddress WiFiHal::getLocalIP() const {
    return WiFi.localIP();
}

int32_t WiFiHal::getRSSI() const {
    return WiFi.RSSI();
}

void WiFiHal::printStatus() const {
    Serial.print("[WiFi] SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WiFi] Signal Strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
}

} // namespace Drivers
} // namespace PlantMonitor