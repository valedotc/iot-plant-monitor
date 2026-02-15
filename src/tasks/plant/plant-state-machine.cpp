#include "plant-state-machine.h"
#include "plant-config.h"
#include "tasks/iot/iot-task-types.h"
#include "utils/timer/periodic-timer.h"
#include <Preferences.h>
#include <time.h>
#include <WiFi.h>

namespace PlantMonitor {
namespace Tasks {

// ============================================================================
// STATIC STATE
// ============================================================================

static PlantState s_currentState = PlantState::PLANT_HAPPY;
static PlantThresholds s_thresholds;
static bool s_thresholdsLoaded = false;
static Utils::PeriodicSendTimer *s_dyingTimer = nullptr;
static bool s_timerStarted = false;

// Debounce tracking
static bool s_lastAllOk = true;
static uint32_t s_lastConditionChangeTime = 0;

// Light tracking
struct LightTracking {
    float accumulatedHours;           // Hours accumulated today
    int lastDayOfYear;                // Day of year (0-365)
    uint8_t daysWithoutEnoughLight;   // Consecutive days without enough light
    uint32_t lastUpdateMs;            // Last update timestamp (millis)
    uint32_t lastDebugPrintMs;        // Last debug print timestamp
    bool initialized;
};

static LightTracking s_lightTracking = {0.0f, -1, 0, 0, 0, false};
static Preferences s_nvs;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/*!
 * \brief Check if internet/WiFi is available
 */
static bool prv_is_internet_available() {
    return WiFi.status() == WL_CONNECTED;
}

/*!
 * \brief Load light tracking data from NVS
 */
static void prv_load_light_tracking_from_nvs() {
    if (!s_nvs.begin("plant_light", true)) { // Read-only
        // Namespace doesn't exist yet (first boot) - use defaults
        Serial.println("[PLANT] NVS namespace not found, using defaults");
        s_lightTracking.accumulatedHours = 0.0f;
        s_lightTracking.lastDayOfYear = -1;
        s_lightTracking.daysWithoutEnoughLight = 0;
        return;
    }

    s_lightTracking.accumulatedHours = s_nvs.getFloat("accum_hours", 0.0f);
    s_lightTracking.lastDayOfYear = s_nvs.getInt("last_day", -1);
    s_lightTracking.daysWithoutEnoughLight = s_nvs.getUChar("days_no_light", 0);
    s_nvs.end();

    Serial.printf("[PLANT] Loaded light tracking: %.1fh accumulated, %d days without light\n",
                 s_lightTracking.accumulatedHours, s_lightTracking.daysWithoutEnoughLight);
}

/*!
 * \brief Save light tracking data to NVS
 */
static void prv_save_light_tracking_to_nvs() {
    if (!s_nvs.begin("plant_light", false)) { // Read-write
        Serial.println("[PLANT] ERROR: Failed to open NVS for writing");
        return;
    }

    s_nvs.putFloat("accum_hours", s_lightTracking.accumulatedHours);
    s_nvs.putInt("last_day", s_lightTracking.lastDayOfYear);
    s_nvs.putUChar("days_no_light", s_lightTracking.daysWithoutEnoughLight);
    s_nvs.end();

    Serial.println("[PLANT] Saved light tracking to NVS");
}

/*!
 * \brief Check if light condition is OK
 */
static bool prv_is_light_ok() {
    if (!s_lightTracking.initialized) {
        return true; // Assume OK until we have data
    }

    // Check based on consecutive days without enough light
    if (s_lightTracking.daysWithoutEnoughLight >= LIGHT_ANGRY_DAYS) {
        return false; // 1+ days -> not OK
    }

    return true;
}

/*!
 * \brief Update light hours tracking
 */
static void prv_update_light_tracking() {
    if (!prv_is_internet_available()) {
        return; // Skip if no internet (can't sync time)
    }

    // Initialize tracking if needed
    if (!s_lightTracking.initialized) {
        prv_load_light_tracking_from_nvs();
        s_lightTracking.lastUpdateMs = millis();
        s_lightTracking.lastDebugPrintMs = 0; // Force first print
        s_lightTracking.initialized = true;
    }

    // Get current time
    time_t now;
    time(&now);
    if (now < 946684800) { // Check if time is valid (> 2000-01-01)
        return; // Time not synced yet
    }

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    int currentDay = timeinfo.tm_yday;

    // Check if day changed (midnight passed)
    if (s_lightTracking.lastDayOfYear != -1 && currentDay != s_lightTracking.lastDayOfYear) {
        // New day - check yesterday's accumulated hours
        if (s_lightTracking.accumulatedHours < s_thresholds.lightMin) {
            s_lightTracking.daysWithoutEnoughLight++;
            Serial.printf("[PLANT] Day ended with %.1fh light (need %.1fh) - %d days without enough light\n",
                         s_lightTracking.accumulatedHours, s_thresholds.lightMin,
                         s_lightTracking.daysWithoutEnoughLight);
        } else {
            s_lightTracking.daysWithoutEnoughLight = 0;
            Serial.printf("[PLANT] Day ended with %.1fh light - Reset counter\n", s_lightTracking.accumulatedHours);
        }

        // Reset for new day
        s_lightTracking.accumulatedHours = 0.0f;
        s_lightTracking.lastDayOfYear = currentDay;
        prv_save_light_tracking_to_nvs();
    } else if (s_lightTracking.lastDayOfYear == -1) {
        // First run
        s_lightTracking.lastDayOfYear = currentDay;
    }

    // Accumulate light hours
    SensorData data;
    if (getLatestSensorData(data)) {
        // Check if plant is getting light
        if (data.lightDetected) {
            uint32_t nowMs = millis();
            uint32_t deltaMs = nowMs - s_lightTracking.lastUpdateMs;
            float deltaHours = deltaMs / 3600000.0f;
            s_lightTracking.accumulatedHours += deltaHours;
        }
        s_lightTracking.lastUpdateMs = millis();

        // Periodic debug print (configured interval)
        uint32_t nowMs = millis();
        if (s_lightTracking.lastDebugPrintMs == 0 ||
            (nowMs - s_lightTracking.lastDebugPrintMs >= LIGHT_DEBUG_INTERVAL_MS)) {
            s_lightTracking.lastDebugPrintMs = nowMs;

            // Get current time for display
            time_t now_time;
            time(&now_time);

            // Only print if time is valid
            if (now_time >= 946684800) {
                struct tm timeinfo;
                localtime_r(&now_time, &timeinfo);

                Serial.println("========================================");
                Serial.printf("[PLANT LIGHT] Debug Info @ %02d:%02d:%02d\n",
                             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                Serial.printf("  Accumulated today: %.2f hours (need %.1f h/day)\n",
                             s_lightTracking.accumulatedHours, s_thresholds.lightMin);
                Serial.printf("  Days without enough light: %d\n",
                             s_lightTracking.daysWithoutEnoughLight);
                Serial.printf("  Currently light detected: %s\n",
                             data.lightDetected ? "YES" : "NO");
                Serial.printf("  Light status: %s\n",
                             prv_is_light_ok() ? "OK" : "BAD");
                Serial.println("========================================");
            }
        }
    }
}

/*!
 * \brief Start the dying state timer
 */
static void prv_start_dying_timer() {
    if (s_dyingTimer && !s_timerStarted) {
        s_dyingTimer->clear();
        s_dyingTimer->start();
        s_timerStarted = true;
        Serial.println("[PLANT] Dying timer started");
    }
}

/*!
 * \brief Stop and reset the dying state timer
 */
static void prv_stop_dying_timer() {
    if (s_dyingTimer && s_timerStarted) {
        s_dyingTimer->stop();
        s_dyingTimer->clear();
        s_timerStarted = false;
        Serial.println("[PLANT] Dying timer stopped");
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void initPlantStateMachine(uint32_t timeoutMinutes) {
    // Use default from config if not specified
    if (timeoutMinutes == 0) {
        timeoutMinutes = DYING_TIMEOUT_MINUTES;
    }

    Serial.printf("[PLANT] Initializing state machine with %lu minute timeout\n", timeoutMinutes);

    // Create timer with specified timeout
    if (s_dyingTimer) {
        delete s_dyingTimer;
    }

    s_dyingTimer = new Utils::PeriodicSendTimer();
    uint32_t timeoutMs = timeoutMinutes * 60 * 1000; // Convert minutes to milliseconds

    if (!s_dyingTimer->begin(timeoutMs, false)) { // Don't start immediately
        Serial.println("[PLANT] ERROR: Failed to initialize dying timer");
        delete s_dyingTimer;
        s_dyingTimer = nullptr;
        return;
    }

    s_currentState = PlantState::PLANT_HAPPY;
    s_timerStarted = false;

    Serial.println("[PLANT] State machine initialized");
}

void updatePlantState() {
    // Load thresholds if not already loaded
    if (!s_thresholdsLoaded) {
        AppConfig cfg;
        if (ConfigHandler::load(cfg)) {
            if (loadThresholdsFromConfig(cfg, s_thresholds)) {
                s_thresholdsLoaded = true;
                Serial.println("[PLANT] Thresholds loaded from configuration");
                Serial.printf("[PLANT] Temp: %.1f-%.1f°C, Humidity: %.1f-%.1f%%, Moisture: %.1f-%.1f%%, Light: %.1fh/day\n",
                              s_thresholds.tempMin, s_thresholds.tempMax,
                              s_thresholds.humidityMin, s_thresholds.humidityMax,
                              s_thresholds.moistureMin, s_thresholds.moistureMax,
                              s_thresholds.lightMin);
            }
        }

        // If still not loaded, return (wait for configuration)
        if (!s_thresholdsLoaded) {
            return;
        }
    }

    // Get current sensor data
    SensorData data;
    if (!getLatestSensorData(data)) {
        return; // No sensor data available
    }

    // Update light tracking (if internet available)
    prv_update_light_tracking();

    // Check if basic sensors are in range
    bool sensorsInRange = areSensorsInRange(data, s_thresholds);

    // Check if light is OK
    bool lightOk = prv_is_light_ok();

    // Combined condition: all must be OK
    bool allOk = sensorsInRange && lightOk;

    uint32_t now = millis();

    // Detect condition change for debounce
    if (allOk != s_lastAllOk) {
        s_lastAllOk = allOk;
        s_lastConditionChangeTime = now;
        Serial.printf("[PLANT] Condition changed: %s (sensors: %s, light: %d days bad)\n",
                     allOk ? "OK" : "BAD",
                     sensorsInRange ? "OK" : "BAD",
                     s_lightTracking.daysWithoutEnoughLight);
    }

    // Calculate time in current condition
    uint32_t timeInCondition = now - s_lastConditionChangeTime;

    // State machine logic with debounce
    PlantState nextState = s_currentState;

    switch (s_currentState) {
        case PlantState::PLANT_HAPPY:
            if (!allOk && timeInCondition >= STATE_DEBOUNCE_MS) {
                // Out of range for 5 minutes -> ANGRY
                nextState = PlantState::PLANT_ANGRY;
                prv_start_dying_timer();
                Serial.println("[PLANT] State: HAPPY -> ANGRY (5 min out of range)");
            }
            break;

        case PlantState::PLANT_ANGRY:
            if (allOk && timeInCondition >= STATE_DEBOUNCE_MS) {
                // Back in range for 5 minutes -> HAPPY
                nextState = PlantState::PLANT_HAPPY;
                prv_stop_dying_timer();
                Serial.println("[PLANT] State: ANGRY -> HAPPY (5 min in range)");
            } else if (!allOk) {
                // Still out of range - check conditions for DYING
                bool shouldGoDying = false;

                // Check if 12h timer expired
                if (s_dyingTimer && s_dyingTimer->take()) {
                    shouldGoDying = true;
                    Serial.println("[PLANT] Dying condition: 12h timer expired");
                }

                // Check if 2+ days without enough light
                if (s_lightTracking.initialized && s_lightTracking.daysWithoutEnoughLight >= LIGHT_DYING_DAYS) {
                    shouldGoDying = true;
                    Serial.printf("[PLANT] Dying condition: %d days without light\n", s_lightTracking.daysWithoutEnoughLight);
                }

                if (shouldGoDying) {
                    nextState = PlantState::PLANT_DYING;
                    Serial.println("[PLANT] State: ANGRY -> DYING");
                }
            }
            break;

        case PlantState::PLANT_DYING:
            if (allOk && timeInCondition >= STATE_DEBOUNCE_MS) {
                // Back in range for 5 minutes -> HAPPY
                nextState = PlantState::PLANT_HAPPY;
                prv_stop_dying_timer();
                Serial.println("[PLANT] State: DYING -> HAPPY (5 min in range)");
            }
            break;
    }

    s_currentState = nextState;
}

PlantState getCurrentPlantState() {
    return s_currentState;
}

bool loadThresholdsFromConfig(const AppConfig &cfg, PlantThresholds &thresholds) {
    if (cfg.params.size() < 8) {
        Serial.printf("[PLANT] ERROR: Insufficient parameters in config (need 8, got %d)\n", cfg.params.size());
        return false;
    }

    // Extract thresholds using ParamIndex enum
    // params[1]=TempMin, params[2]=TempMax, params[3]=HumidityMin, params[4]=HumidityMax,
    // params[5]=MoistureMin, params[6]=MoistureMax, params[7]=LightHoursMin
    thresholds.tempMin = getConfigParam(cfg, ParamIndex::TempMin, 15.0f);
    thresholds.tempMax = getConfigParam(cfg, ParamIndex::TempMax, 30.0f);
    thresholds.humidityMin = getConfigParam(cfg, ParamIndex::HumidityMin, 30.0f);
    thresholds.humidityMax = getConfigParam(cfg, ParamIndex::HumidityMax, 80.0f);
    thresholds.moistureMin = getConfigParam(cfg, ParamIndex::MoistureMin, 20.0f);
    thresholds.moistureMax = getConfigParam(cfg, ParamIndex::MoistureMax, 80.0f);
    thresholds.lightMin = getConfigParam(cfg, ParamIndex::LightHoursMin, 8.0f); // Minimum hours of light per day
    thresholds.lightMax = 24.0f; // No max light parameter in config, use 24 hours

    return true;
}

bool areSensorsInRange(const SensorData &data, const PlantThresholds &thresholds) {
    bool tempOk = (data.temperature >= thresholds.tempMin && data.temperature <= thresholds.tempMax);
    bool humidityOk = (data.humidity >= thresholds.humidityMin && data.humidity <= thresholds.humidityMax);
    bool moistureOk = (data.moisture >= thresholds.moistureMin && data.moisture <= thresholds.moistureMax);
    // Note: Light checking is more complex (requires tracking hours per day), skip for now
    // bool lightOk = data.lightDetected; // Simplified

    return tempOk && humidityOk && moistureOk;
}

} // namespace Tasks
} // namespace PlantMonitor
