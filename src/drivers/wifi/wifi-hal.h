#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <IPAddress.h>
#include <vector>

/*!
 * \file wifi_hal.h
 * \brief Hardware abstraction layer for WiFi connectivity
 *
 * This class handles low-level WiFi operations for ESP32.
 *
 * \note This class is NOT thread-safe. If used from multiple threads,
 * users must provide their own synchronization (e.g., mutexes) to protect WiFi library calls.
 */

#define WIFI_CONNECTION_TIMEOUT_MS 30000 //!< Default WiFi connection timeout in milliseconds
#define WIFI_RETRY_DELAY_MS 500          //!< Default delay between connection attempts in milliseconds
#define WIFI_MAX_RETRIES 20              //!< Default maximum number of connection attempts
#define WIFI_MAX_SCAN_NETWORKS 10        //!< Default maximum number of WiFi networks to return from scan

namespace PlantMonitor {
namespace Drivers {

/*!
 * \struct WiFiNetwork
 * \brief Information about a scanned WiFi network
 */
struct WiFiNetwork {
    String ssid;  //!< Network name
    int32_t rssi; //!< Signal strength in dBm
    bool secure;  //!< true if network requires password
};

/*!
 * \class WiFiHal
 * \brief Hardware abstraction layer for WiFi control
 * 
 * Manages WiFi connection, disconnection, and status monitoring.
 */
class WiFiHal {
  public:
    /*!
     * \brief Constructor
     * \param ssid WiFi network SSID
     * \param password WiFi network password
     * \param max_attempts Maximum connection attempts
     * \param retry_delay_ms Delay between retry attempts in milliseconds
     */
    WiFiHal(const char *ssid, const char *password, int max_attempts = WIFI_MAX_RETRIES, int retry_delay_ms = WIFI_RETRY_DELAY_MS);

    /*!
     * \brief Destructor
     */
    ~WiFiHal() = default;

    /*!
     * \brief Initialize and connect to WiFi network
     * \return true if connection successful, false otherwise
     */
    bool begin();

    /*!
     * \brief Disconnect from WiFi network
     */
    void disconnect();

    /*!
     * \brief Check if WiFi is connected
     * \return true if connected, false otherwise
     */
    bool isConnected() const;

    /*!
     * \brief Get local IP address
     * \return IPAddress object containing the local IP
     */
    IPAddress getLocalIP() const;

    /*!
     * \brief Get WiFi signal strength
     * \return Signal strength in dBm
     */
    int32_t getRSSI() const;

    /*!
     * \brief Print connection status to Serial
     */
    void printStatus() const;

    /*!
     * \brief Scan for available WiFi networks
     * \param maxNetworks Maximum number of networks to return (default 10)
     * \return Vector of WiFiNetwork sorted by signal strength (strongest first)
     * \note This is a static method - can be called without an instance
     * \note Scan takes approximately 2-4 seconds
     */
    static std::vector<WiFiNetwork> scanNetworks(size_t maxNetworks = WIFI_MAX_SCAN_NETWORKS);

  private:
    const char *m_ssid;     //!< WiFi SSID
    const char *m_password; //!< WiFi password
    int m_max_attempts;     //!< Maximum connection attempts
    int m_retry_delay_ms;   //!< Retry delay in milliseconds

    /*!
     * \brief Wait for valid IP address assignment
     * \return true if valid IP obtained, false otherwise
     */
    bool waitForValidIP();

    // Prevent copying
    WiFiHal(const WiFiHal &) = delete;
    WiFiHal &operator=(const WiFiHal &) = delete;
};

} // namespace Drivers
} // namespace PlantMonitor