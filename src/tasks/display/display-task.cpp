#include "display-task.h"

#include "drivers/display/display-hal.h"
#include "../sensor/sensor-task.h"
#include "../plant/plant-state-machine.h"
#include "drivers/sensors/button-sensor/button-sensor-hal.h"
#include "utils/bitmap/bluetooth-icon.h"
#include "utils/bitmap/plant-happy-icon.h"
#include "utils/bitmap/plant-angry-icon.h"
#include "utils/bitmap/plant-dying-icon.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#define SWITCH_PIN 32

using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

/*! \defgroup DisplayTiming Display Timing Configuration
 *  @{
 */
#define UI_UPDATE_INTERVAL_MS 100 /*!< UI refresh interval in milliseconds */
#define UI_PAGE_TIMEOUT_MS 10000  /*!< Page timeout before returning to idle */
/*! @} */

static DisplayHAL *display_task_driver = nullptr;
static UiState display_task_current_state = UiState::UI_STATE_BOOT;

static uint32_t display_task_last_ui_update = 0;
static uint32_t display_task_last_interaction = 0;

static QueueHandle_t display_task_ui_event_queue = nullptr;

PlantMonitor::Drivers::ButtonHal *display_task_button = nullptr;

static bool display_task_plant_state_initialized = false;

/*!
 * \brief ISR callback for boot button press
 * \note This function runs in interrupt context
 */
static void IRAM_ATTR prv_on_boot_button_pressed() {
    if (display_task_ui_event_queue != nullptr) {
        uint8_t evt = 1;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(display_task_ui_event_queue, &evt, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

// ============================================================================
// UI RENDERING - IoT Display (128x128)
// ============================================================================

/*! \defgroup UI_Layout Display Layout Constants
 *  @{
 */
#define HEADER_Y 8   // Header vertical position
#define VALUE_Y 64   // Main value vertical position (centered)
#define UNIT_Y 90    // Unit text vertical position
#define ICON_SIZE 32 // Icon/symbol size
#define ICON_X 64    // Icon horizontal center position
/*! @} */

/*!
 * \brief Draw a centered header with consistent styling
 * \param text Header text to display
 */
static void prv_draw_header(const char *text) {
    display_task_driver->setTextSize(1);
    int16_t x = (128 - strlen(text) * 6) / 2; // Center text (6px per char)
    display_task_driver->setCursor(x, HEADER_Y);
    display_task_driver->printf("%s", text);

    // Subtle divider line
    display_task_driver->drawLine(16, 24, 112, 24, COLOR_WHITE);
}

/*!
 * \brief Draw large centered value with unit
 * \param value Numeric value to display
 * \param unit Unit string (e.g., "°C", "%", "")
 * \param decimals Number of decimal places
 */
static void prv_draw_centered_value(float value, const char *unit, uint8_t decimals = 1) {
    char buffer[16];

    // Format value
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);

    // Draw large value (centered)
    display_task_driver->setTextSize(3);
    int16_t valueWidth = strlen(buffer) * 18;
    int16_t valueX = (128 - valueWidth) / 2;
    display_task_driver->setCursor(valueX, VALUE_Y);
    display_task_driver->printf("%s", buffer);

    if (unit && (unit[0] == 'C' || unit[0] == 'F')) {
        // Calculate width of "°C" (circle + letter)
        int16_t unitTotalWidth = 6 + 12; // circle(6px space) + C(12px)
        int16_t unitStartX = (128 - unitTotalWidth) / 2;

        int16_t degX = unitStartX + 3;
        int16_t degY = UNIT_Y + 4;
        display_task_driver->drawCircle(degX, degY, 3, COLOR_WHITE);

        display_task_driver->setTextSize(2);
        display_task_driver->setCursor(unitStartX + 8, UNIT_Y);
        display_task_driver->printf("%c", unit[0]);
    } else if (unit && strlen(unit) > 0) {
        // Normal unit
        display_task_driver->setTextSize(2);
        int16_t unitWidth = strlen(unit) * 12;
        int16_t unitX = (128 - unitWidth) / 2;
        display_task_driver->setCursor(unitX, UNIT_Y);
        display_task_driver->printf("%s", unit);
    }
}

/*!
 * \brief Draw a simple thermometer icon
 */
static void prv_draw_thermometer_icon() {
    // Thermometer bulb
    display_task_driver->fillCircle(ICON_X, 48, 6, COLOR_WHITE);
    // Thermometer tube
    display_task_driver->fillRect(ICON_X - 2, 32, 4, 17, COLOR_WHITE);
    // Temperature level indicator
    display_task_driver->fillRect(ICON_X - 1, 38, 2, 11, COLOR_BLACK);
}

/*!
 * \brief Draw a water drop icon for humidity
 */
static void prv_draw_droplet_icon() {
    // Simple droplet shape using filled triangle + circle
    display_task_driver->fillCircle(ICON_X, 46, 7, COLOR_WHITE);
    display_task_driver->fillTriangle(
        ICON_X, 32, // top point
        ICON_X - 7,
        46, // bottom left
        ICON_X + 7,
        46, // bottom right
        COLOR_WHITE);
}

/*!
 * \brief Draw a plant/soil icon for moisture
 */
static void prv_draw_plant_icon() {
    // Soil line
    display_task_driver->drawLine(ICON_X - 12, 48, ICON_X + 12, 48, COLOR_WHITE);
    display_task_driver->drawLine(ICON_X - 12, 49, ICON_X + 12, 49, COLOR_WHITE);

    // Simple plant stem
    display_task_driver->drawLine(ICON_X, 48, ICON_X, 36, COLOR_WHITE);
    display_task_driver->drawLine(ICON_X + 1, 48, ICON_X + 1, 36, COLOR_WHITE);

    // Leaves
    display_task_driver->fillCircle(ICON_X - 4, 38, 3, COLOR_WHITE);
    display_task_driver->fillCircle(ICON_X + 5, 40, 3, COLOR_WHITE);
}

/*!
 * \brief Draw status indicator based on value range
 * \param value Current value
 * \param minGood Minimum good range
 * \param maxGood Maximum good range
 */
static void prv_draw_status_indicator(float value, float minGood, float maxGood) {
    int16_t indicatorX = 110;
    int16_t indicatorY = 8;
    int16_t radius = 4;

    // Determine status color (using filled/empty for monochrome)
    if (value >= minGood && value <= maxGood) {
        display_task_driver->fillCircle(indicatorX, indicatorY, radius, COLOR_WHITE);
    } else {
        display_task_driver->drawCircle(indicatorX, indicatorY, radius, COLOR_WHITE);
        display_task_driver->drawCircle(indicatorX, indicatorY, radius - 1, COLOR_WHITE);
    }
}

// ============================================================================
// PAGE RENDERING FUNCTIONS
// ============================================================================

/*!
 * \brief Draw the temperature page with professional layout
 * \param data Current sensor readings
 */
static void prv_draw_temperature(const SensorData &data) {
    display_task_driver->clear();

    prv_draw_header("TEMPERATURE");
    prv_draw_thermometer_icon();
    prv_draw_centered_value(data.temperature, "C", 1);
    prv_draw_status_indicator(data.temperature, 18.0f, 26.0f); // Ideal range: 18-26°C

    display_task_driver->update();
}

/*!
 * \brief Draw the humidity page with professional layout
 * \param data Current sensor readings
 */
static void prv_draw_humidity(const SensorData &data) {
    display_task_driver->clear();

    prv_draw_header("HUMIDITY");
    prv_draw_droplet_icon();
    prv_draw_centered_value(data.humidity, "%", 1);
    prv_draw_status_indicator(data.humidity, 40.0f, 70.0f); // Ideal range: 40-70%

    display_task_driver->update();
}

/*!
 * \brief Draw the soil moisture page with professional layout
 * \param data Current sensor readings
 */
static void prv_draw_moisture(const SensorData &data) {
    display_task_driver->clear();

    prv_draw_header("SOIL MOISTURE");
    prv_draw_plant_icon();

    // Moisture is typically 0-100 scale or raw sensor value
    // Adjust formatting based on your sensor range
    if (data.moisture > 100.0f) {
        prv_draw_centered_value(data.moisture, "", 0); // Raw value, no decimals
    } else {
        prv_draw_centered_value(data.moisture, "%", 0); // Percentage
    }

    prv_draw_status_indicator(data.moisture, 30.0f, 70.0f); // Ideal range: 30-70%

    display_task_driver->update();
}

/*!
 * \brief Draw the idle face with system status (HAPPY state)
 */
static void prv_draw_face_idle() {
    display_task_driver->clear();
    display_task_driver->drawBitmap(plant_happy_bmp);
    display_task_driver->update();
}

/*!
 * \brief Draw angry face (ANGRY state)
 */
static void prv_draw_face_angry() {
    display_task_driver->clear();
    display_task_driver->drawBitmap(plant_angry_bmp);
    display_task_driver->update();
}

/*!
 * \brief Draw dying face (DYING state)
 */
static void prv_draw_face_dying() {
    display_task_driver->clear();
    display_task_driver->drawBitmap(plant_dying_bmp);
    display_task_driver->update();
}

/**
 * \brief Draw the Bluetooth icon
 */
static void prv_draw_bluetooth_icon() {
    display_task_driver->clear();
    display_task_driver->drawBitmap(bluetooth_icon_bmp);

    display_task_driver->update();
}

/*!
 * \brief Get the next UI state in the FSM sequence
 * \param state Current UI state
 * \return Next UI state
 */
static UiState prv_next_state(UiState state) {
    switch (state) {
        case UiState::UI_STATE_FACE_IDLE:
            return UiState::UI_STATE_PAGE_TEMPERATURE;
        case UiState::UI_STATE_PAGE_TEMPERATURE:
            return UiState::UI_STATE_PAGE_HUMIDITY;
        case UiState::UI_STATE_PAGE_HUMIDITY:
            return UiState::UI_STATE_PAGE_MOISTURE;
        case UiState::UI_STATE_PAGE_MOISTURE:
            return UiState::UI_STATE_FACE_IDLE;
        default:
            return UiState::UI_STATE_FACE_IDLE;
    }
}

/*!
 * \brief Main display task function
 * \param pvParameters Task parameters (unused)
 */
static void prv_display_task(void *) {
    display_task_driver = new DisplayHAL();
    if (!display_task_driver->begin()) {
        Serial.println("[DISPLAY] Init failed");
        vTaskDelete(nullptr);
    }

    display_task_driver->setTextSize(1);
    display_task_driver->setTextColor(COLOR_WHITE);
    display_task_last_interaction = millis();

    display_task_ui_event_queue = xQueueCreate(5, sizeof(uint8_t));

    display_task_button = new PlantMonitor::Drivers::ButtonHal(SWITCH_PIN, BUTTON_INPUT_PULLUP, prv_on_boot_button_pressed);

    // Set initial state based on configuration
    if (!ConfigHandler::isConfigured()) {
        display_task_current_state = UiState::UI_STATE_PAIRING;
    }

    uint8_t evt;
    SensorData data;

    while (true) {
        uint32_t now = millis();

        // Initialize plant state machine once device is configured
        if (ConfigHandler::isConfigured() && !display_task_plant_state_initialized) {
            initPlantStateMachine(); // Uses default timeout from plant-config.h
            display_task_plant_state_initialized = true;
            Serial.println("[DISPLAY] Plant state machine initialized");
        }

        // Update plant state machine only if initialized
        if (display_task_plant_state_initialized) {
            updatePlantState();
        }

        if (xQueueReceive(display_task_ui_event_queue, &evt, 0) == pdTRUE) {
            if (display_task_button->debouncing()) {
                display_task_current_state = prv_next_state(display_task_current_state);
                display_task_last_interaction = now;
            }
        }

        if (display_task_current_state != UiState::UI_STATE_FACE_IDLE &&
            now - display_task_last_interaction > UI_PAGE_TIMEOUT_MS && ConfigHandler::isConfigured()) {
            display_task_current_state = UiState::UI_STATE_FACE_IDLE;
        }

        if (now - display_task_last_ui_update >= UI_UPDATE_INTERVAL_MS) {
            display_task_last_ui_update = now;
            getLatestSensorData(data);

            switch (display_task_current_state) {
                case UiState::UI_STATE_PAIRING:
                    prv_draw_bluetooth_icon();
                    break;
                case UiState::UI_STATE_FACE_IDLE: {
                    // Draw face based on plant state
                    PlantState plantState = getCurrentPlantState();
                    switch (plantState) {
                        case PlantState::PLANT_HAPPY:
                            prv_draw_face_idle();
                            break;
                        case PlantState::PLANT_ANGRY:
                            prv_draw_face_angry();
                            break;
                        case PlantState::PLANT_DYING:
                            prv_draw_face_dying();
                            break;
                    }
                    break;
                }
                case UiState::UI_STATE_PAGE_TEMPERATURE:
                    prv_draw_temperature(data);
                    break;
                case UiState::UI_STATE_PAGE_HUMIDITY:
                    prv_draw_humidity(data);
                    break;
                case UiState::UI_STATE_PAGE_MOISTURE:
                    prv_draw_moisture(data);
                    break;
                default:
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void startDisplayTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    xTaskCreatePinnedToCore(
        prv_display_task,
        "DisplayTask",
        stackSize,
        nullptr,
        priority,
        nullptr,
        core);
}

} // namespace Tasks
} // namespace PlantMonitor
