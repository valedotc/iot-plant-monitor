#pragma once

/*!
 * \file plant-config.h
 * \brief Plant State Machine Configuration
 *
 * This file contains all configurable parameters for the plant monitoring state machine.
 * Modify these values to adjust the behavior of the plant health monitoring system.
 */

namespace PlantMonitor {
namespace Tasks {

// ============================================================================
// DEBOUNCE & TIMING CONFIGURATION
// ============================================================================

/*!
 * \brief Debounce time before state changes (minutes)
 *
 * The system waits this long with stable conditions before transitioning
 * between HAPPY, ANGRY, and DYING states.
 *
 * Example: If set to 5, sensors must be out of range for 5 minutes before
 * transitioning from HAPPY to ANGRY.
 *
 * Default: 5 minutes
 * Range: 1-60 minutes
 */
constexpr uint32_t STATE_DEBOUNCE_MINUTES = 1;

/*!
 * \brief Timeout before transitioning to DYING state (minutes)
 *
 * After entering ANGRY state, if conditions don't improve within this time,
 * the plant transitions to DYING state.
 *
 * Testing: 10 minutes (for quick validation)
 * Production: 720 minutes (12 hours)
 *
 * Default: 10 minutes (testing)
 */
constexpr uint32_t DYING_TIMEOUT_MINUTES = 5; // Change to 720 for production

// ============================================================================
// TELEMETRY CONFIGURATION
// ============================================================================

/*!
 * \brief MQTT telemetry publish interval (minutes)
 *
 * How often to send sensor data to MQTT broker.
 * Lower values increase network traffic but provide more frequent updates.
 * Higher values reduce battery consumption and network load.
 *
 * Default: 5 minutes
 * Range: 1-60 minutes
 */
constexpr uint32_t MQTT_TELEMETRY_INTERVAL_MINUTES = 2;

// ============================================================================
// LIGHT TRACKING CONFIGURATION
// ============================================================================

/*!
 * \brief Days without sufficient light before ANGRY state
 *
 * If the plant doesn't receive the minimum daily light hours for this many
 * consecutive days, it transitions to ANGRY state.
 *
 * Default: 1 day
 * Range: 1-7 days
 */
constexpr uint8_t LIGHT_ANGRY_DAYS = 1;

/*!
 * \brief Days without sufficient light before DYING state
 *
 * If the plant doesn't receive the minimum daily light hours for this many
 * consecutive days, it transitions to DYING state.
 *
 * Default: 2 days
 * Range: 2-14 days
 */
constexpr uint8_t LIGHT_DYING_DAYS = 2;

/*!
 * \brief Light detection threshold (percentage)
 *
 * Minimum light percentage (0-100%) to consider light as "detected".
 * This percentage is calculated from ADC readings (0-4095 range).
 *
 * Typical values:
 * - 20-30%: Detects dim ambient light (room with curtains closed)
 * - 40-50%: Normal ambient light threshold (recommended for plants)
 * - 60%+: Only bright direct light
 *
 * Default: 40% (normal ambient light for plant growth)
 * Range: 10-80%
 */
constexpr uint8_t LIGHT_DETECTION_THRESHOLD_PERCENT = 40;

/*!
 * \brief Light tracking debug print interval (minutes)
 *
 * How often to print light tracking statistics to the serial monitor.
 * Set to 0 to disable periodic debug prints.
 *
 * Default: 30 minutes
 * Range: 0 (disabled), 5-1440 minutes (1 day)
 */
constexpr uint32_t LIGHT_DEBUG_INTERVAL_MINUTES = 3;

// ============================================================================
// DERIVED VALUES (DO NOT MODIFY)
// ============================================================================

/*! \brief Debounce time in milliseconds */
constexpr uint32_t STATE_DEBOUNCE_MS = STATE_DEBOUNCE_MINUTES * 60 * 1000;

/*! \brief MQTT telemetry interval in milliseconds */
constexpr uint32_t MQTT_TELEMETRY_INTERVAL_MS = MQTT_TELEMETRY_INTERVAL_MINUTES * 60 * 1000;

/*! \brief Light debug interval in milliseconds */
constexpr uint32_t LIGHT_DEBUG_INTERVAL_MS = LIGHT_DEBUG_INTERVAL_MINUTES * 60 * 1000;

// ============================================================================
// CONFIGURATION NOTES
// ============================================================================

/*
 * TYPICAL CONFIGURATIONS:
 *
 * 1. TESTING (Quick validation):
 *    - STATE_DEBOUNCE_MINUTES = 1
 *    - DYING_TIMEOUT_MINUTES = 5
 *    - MQTT_TELEMETRY_INTERVAL_MINUTES = 1
 *    - LIGHT_DETECTION_THRESHOLD_PERCENT = 30 (sensitive)
 *    - LIGHT_DEBUG_INTERVAL_MINUTES = 3
 *
 * 2. DEVELOPMENT (Reasonable delays):
 *    - STATE_DEBOUNCE_MINUTES = 5
 *    - DYING_TIMEOUT_MINUTES = 10
 *    - MQTT_TELEMETRY_INTERVAL_MINUTES = 5
 *    - LIGHT_DETECTION_THRESHOLD_PERCENT = 40 (normal)
 *    - LIGHT_DEBUG_INTERVAL_MINUTES = 30
 *
 * 3. PRODUCTION (Real-world deployment):
 *    - STATE_DEBOUNCE_MINUTES = 5
 *    - DYING_TIMEOUT_MINUTES = 720 (12 hours)
 *    - MQTT_TELEMETRY_INTERVAL_MINUTES = 15
 *    - LIGHT_DETECTION_THRESHOLD_PERCENT = 40-50 (adjust based on environment)
 *    - LIGHT_DEBUG_INTERVAL_MINUTES = 60
 */

} // namespace Tasks
} // namespace PlantMonitor
