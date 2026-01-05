#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <IPAddress.h>

/*!
 * \file wifi_hal.h
 * \brief Hardware abstraction layer for WiFi connectivity
 * 
 * This class handles low-level WiFi operations for ESP32.
 * 
 * \note This class is NOT thread-safe. If used from multiple threads,
 * users must provide their own synchronization (e.g., mutexes) to protect WiFi library calls.
 */

namespace PlantMonitor {
namespace Drivers {

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
    WiFiHal(const char* ssid, const char* password, 
            int max_attempts = 3, int retry_delay_ms = 500);

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

  private:
    const char* m_ssid;            //!< WiFi SSID
    const char* m_password;        //!< WiFi password
    int m_max_attempts;            //!< Maximum connection attempts
    int m_retry_delay_ms;          //!< Retry delay in milliseconds

    /*!
     * \brief Wait for valid IP address assignment
     * \return true if valid IP obtained, false otherwise
     */
    bool waitForValidIP();

    // Prevent copying
    WiFiHal(const WiFiHal&) = delete;
    WiFiHal& operator=(const WiFiHal&) = delete;
};

} // namespace Drivers
} // namespace PlantMonitor