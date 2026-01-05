#pragma once
#include <string>
#include <vector>
#include <cctype>   // isspace
#include <cstdlib>  // strtof

struct AppConfig {
  std::string ssid;
  std::string password;
  std::vector<float> params;
};


class ConfigHandler{
    public:
        // NVS namespace ("folder" in flash)
        static constexpr const char* kNamespace = "appcfg";

        // Safety limits (avoid huge allocation if flash is corrupted)
        static constexpr std::size_t kMaxParams = 32; // avoid cose sborats

        static bool load(AppConfig& out);
        static bool save(const AppConfig& cfg);
        static bool clear();

        static bool isConfigured();
        static bool setUnconfigured();

        static bool parseAppCfg(const std::string& msg, AppConfig& cfg);

    private:
        static constexpr const char* kKeyOk       = "ok";
        static constexpr const char* kKeySSID     = "ssid";
        static constexpr const char* kKeyPass     = "pass";
        static constexpr const char* kKeyParCount = "p_cnt";
        static constexpr const char* kKeyParBlob  = "p_blob";
};

