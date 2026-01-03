#include "config.h"
#include <Preferences.h>

bool ConfigHandler::load(AppConfig& out) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) return false; // read-only

  const bool ok = prefs.getBool(kKeyOk, false);
  if (!ok) { prefs.end(); return false; }


  if (!prefs.isKey(kKeySSID) || !prefs.isKey(kKeyPass)) {
    prefs.end();
    return false;
  }

  // Strings: Preferences returns Arduino String, convert to std::string
  out.ssid     = prefs.getString(kKeySSID, "").c_str();
  out.password = prefs.getString(kKeyPass, "").c_str();

  // Params count
  const uint32_t count = prefs.getUInt(kKeyParCount, 0);

  // Validate count before allocating
  if (count > kMaxParams) {
    prefs.end();
    return false;
  }

  out.params.clear();
  out.params.resize(count);

  // Read blob (count floats)
  const std::size_t expectedBytes = static_cast<std::size_t>(count) * sizeof(float);

  if (count == 0) {
    // No params saved (allowed)
    prefs.end();
    return true;
  }

  const std::size_t readBytes = prefs.getBytes(kKeyParBlob, out.params.data(), expectedBytes);
  prefs.end();

  // If bytes don't match, treat as invalid/corrupt
  return readBytes == expectedBytes;
}

bool ConfigHandler::save(const AppConfig& cfg) {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false; // read-write

  // Mark invalid first (helps if power dies mid-write)
  prefs.putBool(kKeyOk, false);

  // Save strings
  prefs.putString(kKeySSID, cfg.ssid.c_str());
  prefs.putString(kKeyPass, cfg.password.c_str());

  // Save params count + blob
  const uint32_t count = static_cast<uint32_t>(cfg.params.size());

  if (count > kMaxParams) {
    prefs.end();
    return false;
  }

  prefs.putUInt(kKeyParCount, count);

  const std::size_t bytes = static_cast<std::size_t>(count) * sizeof(float);

  if (count > 0) {
    const std::size_t writtenBytes = prefs.putBytes(kKeyParBlob, cfg.params.data(), bytes);
    if (writtenBytes != bytes) {
      prefs.end();
      return false;
    }
  } else {
    // Optional: remove blob key if no params (not required, but keeps flash tidy)
    prefs.remove(kKeyParBlob);
  }

  // Mark valid last
  prefs.putBool(kKeyOk, true);

  prefs.end();
  return true;
}

bool ConfigHandler::clear() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) return false;
    const bool res = prefs.clear();
    prefs.end();
    return res;
}

bool ConfigHandler::isConfigured() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, true)) {
    return false;
  }

  // 1. Must be explicitly marked valid
  if (!prefs.getBool(kKeyOk, false)) {
    prefs.end();
    return false;
  }

  // 2. Mandatory keys must exist
  if (!prefs.isKey(kKeySSID) || !prefs.isKey(kKeyPass)) {
    prefs.end();
    return false;
  }

  // 3. Mandatory values must be sane
  String ssid = prefs.getString(kKeySSID, "");
  if (ssid.length() == 0) {
    prefs.end();
    return false;
  }

  // 4. Params count must be reasonable
  uint32_t count = prefs.getUInt(kKeyParCount, 0);
  if (count > kMaxParams) {
    prefs.end();
    return false;
  }

  prefs.end();
  return true;
}


bool ConfigHandler::setUnconfigured() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  prefs.putBool(kKeyOk, false);
  prefs.end();
  return true;
}
