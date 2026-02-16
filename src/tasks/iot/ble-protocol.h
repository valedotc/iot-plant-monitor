#pragma once

/*!
 * \file ble-protocol.h
 * \brief BLE protocol handler for IoT task configuration
 *
 * Provides JSON-based command/response protocol over BLE UART.
 * Handles device configuration, WiFi testing, and status reporting.
 */

#include <ArduinoJson.h>
#include "iot-task-types.h"

namespace PlantMonitor {
namespace Drivers {
class BleUartHal;
}

namespace Tasks {

/*!
 * \class BleProtocolHandler
 * \brief Handles BLE command protocol for IoT task configuration
 *
 * This class encapsulates all BLE-related protocol operations:
 * - JSON response building and transmission
 * - Command parsing and dispatch
 * - WiFi scan result formatting
 *
 * Supported commands:
 * - ping: Returns device info (fw_version, hw_version, configured)
 * - get_info: Returns detailed device configuration
 * - wifi_scan: Scans and returns available WiFi networks
 * - config: Saves WiFi and plant configuration
 * - test_wifi: Tests WiFi credentials without saving
 * - reset: Clears all stored configuration
 */
class BleProtocolHandler {
  public:
    /*!
     * \brief Result of command processing
     */
    struct CommandResult {
        IoTState nextState; //!< Next FSM state to transition to
        bool hasConfig;     //!< True if config was parsed
        AppConfig config;   //!< Parsed configuration (if hasConfig)
        String pendingCmd;  //!< Command name for pending operations
    };

    /*!
     * \brief Constructor
     * \param bleController Pointer to the BLE UART HAL (not owned)
     */
    explicit BleProtocolHandler(Drivers::BleUartHal *bleController);

    // ========================================================================
    // Response Senders
    // ========================================================================

    /*!
     * \brief Send a generic JSON document over BLE
     * \param doc JSON document to serialize and send
     */
    void sendJson(JsonDocument &doc);

    /*!
     * \brief Send a JSON document using chunked transfer
     * \param doc JSON document to send
     * \note Used for large payloads exceeding BLE MTU
     */
    void sendJsonChunked(JsonDocument &doc);

    /*!
     * \brief Send pong response with device information
     */
    void sendPong();

    /*!
     * \brief Send detailed device info response
     * \param deviceId Current device ID
     */
    void sendInfo(int deviceId);

    /*!
     * \brief Send command acknowledgment
     * \param cmd Command name being acknowledged
     */
    void sendAck(const char *cmd);

    /*!
     * \brief Send status update with optional progress
     * \param state State description string
     * \param progress Progress percentage (0-100), -1 to omit
     */
    void sendStatus(const char *state, int progress = -1);

    /*!
     * \brief Send command result (success or error)
     * \param cmd Command that was executed
     * \param success True if successful
     * \param error Optional error code
     * \param msg Optional human-readable message
     */
    void sendResult(const char *cmd, bool success, const char *error = nullptr, const char *msg = nullptr);

    /*!
     * \brief Scan and send WiFi network list
     */
    void sendWifiList();

    // ========================================================================
    // Command Handling
    // ========================================================================

    /*!
     * \brief Process a BLE command and determine next action
     * \param data Raw command string from BLE
     * \param currentDeviceId Current device ID for context
     * \return CommandResult with next state and any pending configuration
     */
    CommandResult handleCommand(const char *data, int currentDeviceId);

    /*!
     * \brief Parse configuration from JSON document
     * \param doc JSON document containing config
     * \param cfg Output configuration structure
     * \return true if parsing succeeded
     */
    static bool parseConfigFromJson(JsonDocument &doc, AppConfig &cfg);

  private:
    Drivers::BleUartHal *m_ble; //!< BLE UART controller (not owned)
};

} // namespace Tasks
} // namespace PlantMonitor
