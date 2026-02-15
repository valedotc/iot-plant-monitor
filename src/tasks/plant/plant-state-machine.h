#pragma once

#include <Arduino.h>
#include "tasks/sensor/sensor-task.h"
#include "utils/configuration/config.h"

namespace PlantMonitor {
namespace Tasks {

/*!
 * \enum PlantState
 * \brief States of the plant health monitoring FSM
 */
enum class PlantState {
    PLANT_HAPPY,   //!< All sensors within valid range
    PLANT_ANGRY,   //!< At least one sensor out of range
    PLANT_DYING    //!< Out of range for more than configured timeout
};

/*!
 * \struct PlantThresholds
 * \brief Threshold configuration for plant health monitoring
 */
struct PlantThresholds {
    float tempMin;         //!< Minimum temperature threshold (°C)
    float tempMax;         //!< Maximum temperature threshold (°C)
    float humidityMin;     //!< Minimum air humidity threshold (%)
    float humidityMax;     //!< Maximum air humidity threshold (%)
    float moistureMin;     //!< Minimum soil moisture threshold (%)
    float moistureMax;     //!< Maximum soil moisture threshold (%)
    float lightMin;        //!< Minimum light hours required
    float lightMax;        //!< Maximum light hours allowed
};

/*!
 * \brief Initialize the plant state machine
 * \param timeoutMinutes Timeout in minutes before transitioning to DYING state
 * \note Default timeout is defined in plant-config.h (DYING_TIMEOUT_MINUTES)
 * \note If not specified, uses default from configuration
 */
void initPlantStateMachine(uint32_t timeoutMinutes = 0);

/*!
 * \brief Update the plant state machine
 * \note Must be called periodically (non-blocking)
 */
void updatePlantState();

/*!
 * \brief Get the current plant state
 * \return Current PlantState
 */
PlantState getCurrentPlantState();

/*!
 * \brief Load thresholds from configuration
 * \param cfg Application configuration
 * \param[out] thresholds Parsed thresholds
 * \return true if thresholds loaded successfully
 */
bool loadThresholdsFromConfig(const AppConfig &cfg, PlantThresholds &thresholds);

/*!
 * \brief Check if all sensor values are within configured thresholds
 * \param data Current sensor readings
 * \param thresholds Configured thresholds
 * \return true if all values are within range, false otherwise
 */
bool areSensorsInRange(const SensorData &data, const PlantThresholds &thresholds);

/*!
 * \brief Convert PlantState to string representation
 * \param state Plant state to convert
 * \return String representation
 */
inline const char *plantStateToString(PlantState state) {
    switch (state) {
        case PlantState::PLANT_HAPPY:
            return "HAPPY";
        case PlantState::PLANT_ANGRY:
            return "ANGRY";
        case PlantState::PLANT_DYING:
            return "DYING";
        default:
            return "UNKNOWN";
    }
}

} // namespace Tasks
} // namespace PlantMonitor
