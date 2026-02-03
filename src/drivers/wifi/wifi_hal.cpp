#include "wifi_hal.h"
#include <algorithm>

namespace PlantMonitor {
namespace Drivers {

std::vector<WiFiNetwork> WiFiHal::scanNetworks(size_t maxNetworks) {
    std::vector<WiFiNetwork> networks;

    Serial.println("[WiFi] Starting network scan...");

    int n = WiFi.scanNetworks(false, false); // sync, no hidden networks

    if (n <= 0) {
        Serial.println("[WiFi] No networks found");
        return networks;
    }

    Serial.printf("[WiFi] Found %d networks\n", n);

    // Collect all networks
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);

        // Skip empty SSIDs (hidden networks)
        if (ssid.length() == 0) continue;

        // Skip duplicates (same SSID on different channels)
        bool duplicate = false;
        for (const auto& net : networks) {
            if (net.ssid == ssid) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        WiFiNetwork net;
        net.ssid = ssid;
        net.rssi = WiFi.RSSI(i);
        net.secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        networks.push_back(net);
    }

    // Sort by signal strength (strongest first)
    std::sort(networks.begin(), networks.end(), [](const WiFiNetwork& a, const WiFiNetwork& b) {
        return a.rssi > b.rssi;
    });

    // Limit to maxNetworks
    if (networks.size() > maxNetworks) {
        networks.resize(maxNetworks);
    }

    // Cleanup scan results from memory
    WiFi.scanDelete();

    Serial.printf("[WiFi] Returning %d unique networks\n", networks.size());
    return networks;
}

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