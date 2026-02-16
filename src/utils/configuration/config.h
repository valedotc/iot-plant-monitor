#pragma once

#include <string>
#include <vector>
#include <cctype>  // std::isspace (used by parser implementation)
#include <cstdlib> // std::strtof  (used by parser implementation)

/*!
 * \file config.h
 * \brief Application configuration types and persistence/parsing utilities.
 *
 * This module defines:
 * - AppConfig: configuration payload (Wi-Fi credentials + numeric parameters)
 * - ConfigHandler: static helper API to load/save/clear configuration in ESP32 NVS
 *   and to parse configuration from a textual message.
 *
 * Storage model (NVS/Preferences):
 * - Data is stored under ConfigHandler::kNamespace.
 * - A boolean validity marker ("ok") is used to determine whether the configuration
 *   is complete and safe to use.
 */

/// \brief Application configuration payload.
struct AppConfig {

    /// \brief Wi-Fi SSID.
    std::string ssid;

    /// \brief Wi-Fi password.
    std::string password;

    /*!
   * \brief Optional numeric parameters.
   *
   * The meaning/order of these values is application-specific.
   */
    std::vector<float> params;
};

/*!
 * \class ConfigHandler
 * \brief Static utility class to manage AppConfig storage and parsing.
 *
 * Responsibilities:
 * - Persist AppConfig in ESP32 NVS (via Preferences)
 * - Clear stored data and/or invalidate the configuration flag
 * - Parse AppConfig from a compact, JSON-like textual representation
 *
 * Configuration validity:
 * - A config is considered "configured" only if the internal "ok" key is true
 *   AND mandatory keys exist (SSID and password).
 *
 * \note This class is not meant to be instantiated (all methods are static).
 */
class ConfigHandler {
  public:
    /*!
     * \brief NVS namespace used to store configuration keys.
     *
     * In ESP32 Preferences, the namespace groups keys similarly to a folder.
     */
    static constexpr const char *kNamespace = "appcfg";

    /*!
     * \brief Maximum number of params allowed.
     *
     * Safety limit to avoid huge allocations if flash is corrupted or the input
     * message contains too many floats.
     */
    static constexpr std::size_t kMaxParams = 32;

    /*!
     * \brief Load configuration from NVS into @p out.
     * \param[out] out Destination structure filled with stored values.
     * \return true if a valid configuration is present and loaded, false otherwise.
     *
     * A valid configuration requires:
     * - "ok" flag == true
     * - SSID and password keys exist
     * - params count <= kMaxParams
     */
    static bool load(AppConfig &out);

    /*!
     * \brief Save configuration to NVS.
     * \param cfg Configuration to persist.
     * \return true on success, false on failure.
     *
     * Typical implementation detail:
     * - Writes "ok" = false first
     * - Writes all values (SSID, password, params count/blob)
     * - Writes "ok" = true last (commit marker)
     *
     * \note Saving fails if cfg.params.size() > kMaxParams.
     */
    static bool save(const AppConfig &cfg);

    /*!
     * \brief Clear all configuration keys from the NVS namespace.
     * \return true if cleared successfully, false otherwise.
     */
    static bool clear();

    /*!
     * \brief Check whether the device is configured.
     * \return true if configuration is present and valid, false otherwise.
     *
     * This typically checks:
     * - "ok" flag is true
     * - SSID/password exist and SSID is non-empty
     * - params count is within bounds
     */
    static bool isConfigured();

    /*!
     * \brief Mark the configuration as unconfigured by clearing the validity flag.
     * \return true on success, false otherwise.
     *
     * \note This does not necessarily erase SSID/password/params keys; it only
     *       marks the configuration as invalid so the device will require setup again.
     */
    static bool setUnconfigured();

    /*!
     * \brief Parse AppConfig from a compact array-like message.
     * \param msg Input message to parse.
     * \param[out] cfg Parsed configuration (reset before filling).
     * \return true if parsing succeeds, false otherwise.
     * Expected format (JSON-like array):
     * \code
     * ["<ssid>","<password>", <float1>, <float2>, ...]
     * \endcode
     *
     * Rules:
     * - The first element must be a quoted SSID string.
     * - The second element must be a quoted password string.
     * - Remaining elements are optional floats, separated by commas, until ']'.
     *
     * Examples:
     * \code
     * ["HomeWiFi","secret"]
     * ["HomeWiFi","secret", 1.0, 2.5, -3]
     * \endcode
     *
     * Escape handling inside strings is minimal (backslash copies next char).
     *
     * \warning This is a lightweight parser and is NOT a full JSON parser.
     */
    static bool parseAppCfg(const std::string &msg, AppConfig &cfg);

  private:
    /// \brief NVS key used as "configuration valid" marker.
    static constexpr const char *kKeyOk = "ok";

    /// \brief NVS key for the Wi-Fi SSID.
    static constexpr const char *kKeySSID = "ssid";

    /// \brief NVS key for the Wi-Fi password.
    static constexpr const char *kKeyPass = "pass";

    /// \brief NVS key for the stored number of float parameters.
    static constexpr const char *kKeyParCount = "p_cnt";

    /*!
     * \brief NVS key for the raw parameters blob.
     *
     * Implementation detail: params can be stored as a contiguous float array (bytes).
     */
    static constexpr const char *kKeyParBlob = "p_blob";
};
