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

void setConfigure(){
  
}

bool ConfigHandler::setUnconfigured() {
  Preferences prefs;
  if (!prefs.begin(kNamespace, false)) return false;
  prefs.putBool(kKeyOk, false);
  prefs.end();
  return true;
}

static void skipSpaces(const char*& p) {
	while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
}

static bool consumeChar(const char*& p, char ch) {
	skipSpaces(p);
	if (*p == ch) { ++p; return true; }
	return false;
}

static bool parseQuotedString(const char*& p, std::string& out) {
	skipSpaces(p);
  if (*p != '"') return false;
  ++p; // skip opening quote
  
  out.clear();
  while (*p) {
	  char c = *p++;
	  if (c == '"') return true;           // end string
	  if (c == '\\' && *p) {               // simple escape support
		out += *p++;
    } else {
		out += c;
    }
}
return false; // no closing quote
}

static bool parseFloat(const char*& p, float& out) {
	skipSpaces(p);
	char* endPtr = nullptr;
	out = std::strtof(p, &endPtr);
	if (endPtr == p) return false; // no conversion
	p = endPtr;
	return true;
}

bool ConfigHandler::parseAppCfg(const std::string& msg, AppConfig& cfg){
  
  cfg = AppConfig{}; // reset
  const char* p = msg.c_str();

  if (!consumeChar(p, '{')) return false;

  // Parse key-value pairs
  bool foundSsid = false, foundPass = false;
  
  while (true) {
    skipSpaces(p);
    if (consumeChar(p, '}')) break;
    
    if (foundSsid || foundPass) {
      if (!consumeChar(p, ',')) return false;
    }
    
    std::string key;
    if (!parseQuotedString(p, key)) return false;
    if (!consumeChar(p, ':')) return false;
    
    if (key == "cmd") {
      std::string cmd;
      if (!parseQuotedString(p, cmd)) return false;
      // Optionally validate cmd == "config"
    } else if (key == "ssid") {
      if (!parseQuotedString(p, cfg.ssid)) return false;
      foundSsid = true;
    } else if (key == "pass") {
      if (!parseQuotedString(p, cfg.password)) return false;
      foundPass = true;
    } else if (key == "params") {
      if (!consumeChar(p, '[')) return false;
      skipSpaces(p);
      if (!consumeChar(p, ']')) { // non-empty array
        while (true) {
          float v = 0.0f;
          if (!parseFloat(p, v)) return false;
          cfg.params.push_back(v);
          skipSpaces(p);
          if (consumeChar(p, ']')) break;
          if (!consumeChar(p, ',')) return false;
        }
      }
    } else {
      // Unknown key, skip value (simple skip for string or number)
      skipSpaces(p);
      if (*p == '"') {
        std::string dummy;
        if (!parseQuotedString(p, dummy)) return false;
      } else {
        float dummy;
        if (!parseFloat(p, dummy)) return false;
      }
    }
  }
  
  return foundSsid && foundPass;
}