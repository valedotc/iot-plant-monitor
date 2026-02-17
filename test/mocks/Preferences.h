#pragma once
/*!
 * \file Preferences.h
 * \brief Mock ESP32 Preferences (NVS) for native unit testing.
 *
 * Uses an in-memory map as backing store so ConfigHandler::load/save/clear
 * can be tested without real flash.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <map>
#include <vector>

#include "Arduino.h" // for String

class Preferences {
  public:
    bool begin(const char *ns, bool readOnly = false) {
        m_ns = ns;
        m_readOnly = readOnly;
        return true;
    }

    void end() {}

    bool clear() {
        store().clear();
        return true;
    }

    bool remove(const char *key) {
        store().erase(key);
        return true;
    }

    bool isKey(const char *key) {
        return store().count(key) > 0;
    }

    // ---- Bool ----
    bool putBool(const char *key, bool value) {
        uint8_t v = value ? 1 : 0;
        store()[key] = {v};
        return true;
    }

    bool getBool(const char *key, bool defaultValue = false) {
        auto it = store().find(key);
        if (it == store().end() || it->second.empty()) return defaultValue;
        return it->second[0] != 0;
    }

    // ---- String ----
    size_t putString(const char *key, const char *value) {
        std::string s(value);
        store()[key] = std::vector<uint8_t>(s.begin(), s.end());
        return s.size();
    }

    String getString(const char *key, const char *defaultValue = "") {
        auto it = store().find(key);
        if (it == store().end()) return String(defaultValue);
        std::string s(it->second.begin(), it->second.end());
        return String(s.c_str());
    }

    // ---- UInt ----
    size_t putUInt(const char *key, uint32_t value) {
        auto &v = store()[key];
        v.resize(sizeof(uint32_t));
        std::memcpy(v.data(), &value, sizeof(uint32_t));
        return sizeof(uint32_t);
    }

    uint32_t getUInt(const char *key, uint32_t defaultValue = 0) {
        auto it = store().find(key);
        if (it == store().end() || it->second.size() < sizeof(uint32_t)) return defaultValue;
        uint32_t val;
        std::memcpy(&val, it->second.data(), sizeof(uint32_t));
        return val;
    }

    // ---- Float ----
    size_t putFloat(const char *key, float value) {
        auto &v = store()[key];
        v.resize(sizeof(float));
        std::memcpy(v.data(), &value, sizeof(float));
        return sizeof(float);
    }

    float getFloat(const char *key, float defaultValue = 0.0f) {
        auto it = store().find(key);
        if (it == store().end() || it->second.size() < sizeof(float)) return defaultValue;
        float val;
        std::memcpy(&val, it->second.data(), sizeof(float));
        return val;
    }

    // ---- Int ----
    size_t putInt(const char *key, int32_t value) {
        auto &v = store()[key];
        v.resize(sizeof(int32_t));
        std::memcpy(v.data(), &value, sizeof(int32_t));
        return sizeof(int32_t);
    }

    int32_t getInt(const char *key, int32_t defaultValue = 0) {
        auto it = store().find(key);
        if (it == store().end() || it->second.size() < sizeof(int32_t)) return defaultValue;
        int32_t val;
        std::memcpy(&val, it->second.data(), sizeof(int32_t));
        return val;
    }

    // ---- UChar ----
    size_t putUChar(const char *key, uint8_t value) {
        store()[key] = {value};
        return 1;
    }

    uint8_t getUChar(const char *key, uint8_t defaultValue = 0) {
        auto it = store().find(key);
        if (it == store().end() || it->second.empty()) return defaultValue;
        return it->second[0];
    }

    // ---- Bytes (blob) ----
    size_t putBytes(const char *key, const void *data, size_t len) {
        auto &v = store()[key];
        v.resize(len);
        std::memcpy(v.data(), data, len);
        return len;
    }

    size_t getBytes(const char *key, void *buf, size_t maxLen) {
        auto it = store().find(key);
        if (it == store().end()) return 0;
        size_t toCopy = std::min(maxLen, it->second.size());
        std::memcpy(buf, it->second.data(), toCopy);
        return toCopy;
    }

  private:
    using Store = std::map<std::string, std::vector<uint8_t>>;

    // Global per-namespace storage (persists across Preferences instances)
    static std::map<std::string, Store> &globalStore() {
        static std::map<std::string, Store> s;
        return s;
    }

    Store &store() { return globalStore()[m_ns]; }

    std::string m_ns;
    bool m_readOnly = false;

  public:
    /// Clear all namespaces (call in setUp to reset between tests)
    static void resetAllMockStorage() { globalStore().clear(); }
};
