#pragma once

#include <Arduino.h>
#include "utils/configuration/config.h"

namespace PlantMonitor {
namespace Tasks {

/*!
 * \brief UI modes handled by the display FSM
 */
enum class UiState {
    UI_STATE_BOOT,
    UI_STATE_PAIRING,
    UI_STATE_FACE_IDLE,
    UI_STATE_PAGE_TEMPERATURE,
    UI_STATE_PAGE_HUMIDITY,
    UI_STATE_PAGE_MOISTURE
};

/*!
 * \brief Starts the display task
 * \param stackSize Task stack size
 * \param priority Task priority
 * \param core ESP32 core to pin the task to
 */
void startDisplayTask(
    uint32_t stackSize = 4096,
    UBaseType_t priority = 3,
    BaseType_t core = 1);

/*!
 * \brief Notifies the display task of a button press
 */
void notifyButtonPress();

} // namespace Tasks
} // namespace PlantMonitor
