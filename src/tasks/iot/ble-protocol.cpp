/*!
 * \file ble-protocol.cpp
 * \brief Implementation of BLE protocol handler
 */

#include "ble-protocol.h"
#include "drivers/bluetooth/bluetooth-hal.h"
#include "drivers/wifi/wifi-hal.h"
#include "utils/configuration/config.h"
#include <cstring>

using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

// ============================================================================
// CONSTRUCTOR
// ============================================================================

BleProtocolHandler::BleProtocolHandler(BleUartHal *bleController)
    : m_ble(bleController) {
}

// ============================================================================
// JSON SENDERS
// ============================================================================

void BleProtocolHandler::sendJson(JsonDocument &doc) {
    String json;
    serializeJson(doc, json);
    Serial.printf("[BLE] TX: %s\n", json.c_str());
    m_ble->sendText(json.c_str());
}

void BleProtocolHandler::sendJsonChunked(JsonDocument &doc) {
    String json;
    serializeJson(doc, json);
    Serial.printf("[BLE] TX (chunked): %s\n", json.c_str());
    m_ble->sendTextChunked(json.c_str());
}

// ============================================================================
// RESPONSE BUILDERS
// ============================================================================

void BleProtocolHandler::sendPong() {
    JsonDocument doc;
    doc["type"] = "pong";
    doc["fw_version"] = IOT_FW_VERSION;
    doc["hw_version"] = IOT_HW_VERSION;
    doc["configured"] = ConfigHandler::isConfigured();
    sendJson(doc);
}

void BleProtocolHandler::sendInfo(int deviceId) {
    JsonDocument doc;
    doc["type"] = "info";
    doc["fw_version"] = IOT_FW_VERSION;
    doc["device_id"] = deviceId;
    doc["configured"] = ConfigHandler::isConfigured();

    AppConfig cfg;
    if (ConfigHandler::load(cfg)) {
        doc["wifi_ssid"] = cfg.ssid.c_str();
        if (!cfg.params.empty()) {
            doc["plant_type"] = static_cast<int>(getConfigParam(cfg, ParamIndex::PlantTypeId));
        }
    }

    sendJson(doc);
}

void BleProtocolHandler::sendAck(const char *cmd) {
    JsonDocument doc;
    doc["type"] = "ack";
    doc["cmd"] = cmd;
    sendJson(doc);
}

void BleProtocolHandler::sendStatus(const char *state, int progress) {
    JsonDocument doc;
    doc["type"] = "status";
    doc["state"] = state;
    if (progress >= 0) {
        doc["progress"] = progress;
    }
    sendJson(doc);
}

void BleProtocolHandler::sendResult(const char *cmd, bool success, const char *error, const char *msg) {
    JsonDocument doc;
    doc["type"] = "result";
    doc["cmd"] = cmd;
    doc["status"] = success ? "ok" : "error";
    if (error)
        doc["error"] = error;
    if (msg)
        doc["msg"] = msg;
    sendJson(doc);
}

void BleProtocolHandler::sendWifiList() {
    auto networks = WiFiHal::scanNetworks(8);

    JsonDocument doc;
    doc["type"] = "wifi_list";
    JsonArray arr = doc["networks"].to<JsonArray>();

    for (const auto &net : networks) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["rssi"] = net.rssi;
        obj["secure"] = net.secure;
    }

    sendJsonChunked(doc);
}

// ============================================================================
// CONFIG PARSING
// ============================================================================

bool BleProtocolHandler::parseConfigFromJson(JsonDocument &doc, AppConfig &cfg) {
    if (!doc["ssid"].is<const char *>() || !doc["pass"].is<const char *>()) {
        return false;
    }

    cfg.ssid = doc["ssid"].as<const char *>();
    cfg.password = doc["pass"].as<const char *>();
    cfg.params.clear();

    if (doc["params"].is<JsonArray>()) {
        JsonArray params = doc["params"].as<JsonArray>();
        for (JsonVariant v : params) {
            cfg.params.push_back(v.as<float>());
        }
    }

    return true;
}

// ============================================================================
// COMMAND HANDLER
// ============================================================================

BleProtocolHandler::CommandResult BleProtocolHandler::handleCommand(
    const char *data, int currentDeviceId) {

    CommandResult result;
    result.nextState = IoTState::BleConfiguring;
    result.hasConfig = false;
    result.pendingCmd = "";

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data);

    if (error) {
        Serial.printf("[BLE] JSON parse error: %s\n", error.c_str());
        sendResult("unknown", false, "parse_error", "JSON non valido");
        return result;
    }

    const char *cmd = doc["cmd"] | "unknown";

    // PING
    if (strcmp(cmd, "ping") == 0) {
        sendPong();
        return result;
    }

    // WIFI_SCAN
    if (strcmp(cmd, "wifi_scan") == 0) {
        sendAck("wifi_scan");
        sendWifiList();
        return result;
    }

    // GET_INFO
    if (strcmp(cmd, "get_info") == 0) {
        sendInfo(currentDeviceId);
        return result;
    }

    // CONFIG
    if (strcmp(cmd, "config") == 0) {
        AppConfig cfg;
        if (parseConfigFromJson(doc, cfg)) {
            sendAck("config");
            sendStatus("saving_config");
            sendStatus("connecting_wifi", 0);

            result.nextState = IoTState::BleTestingWifi;
            result.hasConfig = true;
            result.config = cfg;
            result.pendingCmd = "config";
            return result;
        } else {
            sendResult("config", false, "invalid_params", "Missing parameters");
            return result;
        }
    }

    // TEST_WIFI
    if (strcmp(cmd, "test_wifi") == 0) {
        if (doc["ssid"].is<const char *>() && doc["pass"].is<const char *>()) {
            sendAck("test_wifi");

            result.nextState = IoTState::BleTestingWifi;
            result.hasConfig = false;
            result.config.ssid = doc["ssid"].as<const char *>();
            result.config.password = doc["pass"].as<const char *>();
            result.config.params.clear();
            result.pendingCmd = "test_wifi";
            return result;
        } else {
            sendResult("test_wifi", false, "invalid_params", "missing SSID or password");
            return result;
        }
    }

    // RESET
    if (strcmp(cmd, "reset") == 0) {
        sendAck("reset");
        ConfigHandler::clear();
        sendResult("reset", true);
        result.nextState = IoTState::BleAdvertising;
        return result;
    }

    // UNKNOWN
    sendResult(cmd, false, "unknown_cmd", "Unknown command");
    return result;
}

} // namespace Tasks
} // namespace PlantMonitor
