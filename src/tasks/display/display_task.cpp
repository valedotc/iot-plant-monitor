#include "display_task.h"

#include "drivers/display/display_hal.h"
#include "../sensor/sensor_task.h"
#include "drivers/sensors/button-sensor/button_sensor.h"  


#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

using namespace PlantMonitor::Drivers;

namespace PlantMonitor {
namespace Tasks {

/*! \defgroup DisplayTiming Display Timing Configuration
 *  @{
 */
#define UI_UPDATE_INTERVAL_MS 100   /*!< UI refresh interval in milliseconds */
#define UI_PAGE_TIMEOUT_MS   10000   /*!< Page timeout before returning to idle */
/*! @} */

static DisplayHAL* displayDriver = nullptr;
static UiState currentState = UiState::BOOT;

static uint32_t lastUiUpdate = 0;
static uint32_t lastInteraction = 0;

static QueueHandle_t uiEventQueue = nullptr;

static PlantMonitor::Drivers::ButtonHal button;


/*!
 * \brief ISR callback for boot button press
 * \note This function runs in interrupt context
 */
static void IRAM_ATTR onBootButtonPressed() {
    if (uiEventQueue != nullptr) {
        uint8_t evt = 1;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(uiEventQueue, &evt, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

/*!
 * \brief Initialize the boot button with interrupt
 */
static void initButton() {
    pinMode(32, INPUT_PULLUP);
    attachInterrupt(32, onBootButtonPressed, FALLING);
}

// ============================================================================
// UI RENDERING - Professional IoT Display (128x128)
// ============================================================================

/*! \defgroup UI_Layout Display Layout Constants
 *  @{
 */
#define HEADER_Y        8      // Header vertical position
#define VALUE_Y         64     // Main value vertical position (centered)
#define UNIT_Y          90     // Unit text vertical position
#define ICON_SIZE       32     // Icon/symbol size
#define ICON_X          64     // Icon horizontal center position
/*! @} */

/*!
 * \brief Draw a centered header with consistent styling
 * \param text Header text to display
 */
static void drawHeader(const char* text) {
    displayDriver->setTextSize(1);
    int16_t x = (128 - strlen(text) * 6) / 2; // Center text (6px per char)
    displayDriver->setCursor(x, HEADER_Y);
    displayDriver->printf("%s", text);
    
    // Subtle divider line
    displayDriver->drawLine(16, 24, 112, 24, COLOR_WHITE);
}

/*!
 * \brief Draw large centered value with unit
 * \param value Numeric value to display
 * \param unit Unit string (e.g., "°C", "%", "")
 * \param decimals Number of decimal places
 */
static void drawCenteredValue(float value, const char* unit, uint8_t decimals = 1) {
    char buffer[16];
    
    // Format value
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    
    // Draw large value (centered)
    displayDriver->setTextSize(3);
    int16_t valueWidth = strlen(buffer) * 18;
    int16_t valueX = (128 - valueWidth) / 2;
    displayDriver->setCursor(valueX, VALUE_Y);
    displayDriver->printf("%s", buffer);
    
    if (unit && (unit[0] == 'C' || unit[0] == 'F')) {
        // Calcola larghezza di "°C" (cerchio + lettera)
        int16_t unitTotalWidth = 6 + 12;  // cerchio(6px spazio) + C(12px)
        int16_t unitStartX = (128 - unitTotalWidth) / 2;
        
        int16_t degX = unitStartX + 3;
        int16_t degY = UNIT_Y + 4;
        displayDriver->drawCircle(degX, degY, 3, COLOR_WHITE);
        
        displayDriver->setTextSize(2);
        displayDriver->setCursor(unitStartX + 8, UNIT_Y);
        displayDriver->printf("%c", unit[0]);
    } else if (unit && strlen(unit) > 0) {
        // Normal unit
        displayDriver->setTextSize(2);
        int16_t unitWidth = strlen(unit) * 12;
        int16_t unitX = (128 - unitWidth) / 2;
        displayDriver->setCursor(unitX, UNIT_Y);
        displayDriver->printf("%s", unit);
    }
}

/*!
 * \brief Draw a simple thermometer icon
 */
static void drawThermometerIcon() {
    // Thermometer bulb
    displayDriver->fillCircle(ICON_X, 48, 6, COLOR_WHITE);
    // Thermometer tube
    displayDriver->fillRect(ICON_X - 2, 32, 4, 17, COLOR_WHITE);
    // Temperature level indicator
    displayDriver->fillRect(ICON_X - 1, 38, 2, 11, COLOR_BLACK);
}

/*!
 * \brief Draw a water drop icon for humidity
 */
static void drawDropletIcon() {
    // Simple droplet shape using filled triangle + circle
    displayDriver->fillCircle(ICON_X, 46, 7, COLOR_WHITE);
    displayDriver->fillTriangle(
        ICON_X, 32,      // top point
        ICON_X - 7, 46,  // bottom left
        ICON_X + 7, 46,  // bottom right
        COLOR_WHITE
    );
}

/*!
 * \brief Draw a plant/soil icon for moisture
 */
static void drawPlantIcon() {
    // Soil line
    displayDriver->drawLine(ICON_X - 12, 48, ICON_X + 12, 48, COLOR_WHITE);
    displayDriver->drawLine(ICON_X - 12, 49, ICON_X + 12, 49, COLOR_WHITE);
    
    // Simple plant stem
    displayDriver->drawLine(ICON_X, 48, ICON_X, 36, COLOR_WHITE);
    displayDriver->drawLine(ICON_X + 1, 48, ICON_X + 1, 36, COLOR_WHITE);
    
    // Leaves
    displayDriver->fillCircle(ICON_X - 4, 38, 3, COLOR_WHITE);
    displayDriver->fillCircle(ICON_X + 5, 40, 3, COLOR_WHITE);
}

/*!
 * \brief Draw status indicator based on value range
 * \param value Current value
 * \param minGood Minimum good range
 * \param maxGood Maximum good range
 */
static void drawStatusIndicator(float value, float minGood, float maxGood) {
    int16_t indicatorX = 110;
    int16_t indicatorY = 8;
    int16_t radius = 4;
    
    // Determine status color (using filled/empty for monochrome)
    if (value >= minGood && value <= maxGood) {
        displayDriver->fillCircle(indicatorX, indicatorY, radius, COLOR_WHITE);
    } else {
        displayDriver->drawCircle(indicatorX, indicatorY, radius, COLOR_WHITE);
        displayDriver->drawCircle(indicatorX, indicatorY, radius - 1, COLOR_WHITE);
    }
}

// ============================================================================
// PAGE RENDERING FUNCTIONS
// ============================================================================

/*!
 * \brief Draw the temperature page with professional layout
 * \param data Current sensor readings
 */
static void drawTemperature(const SensorData& data) {
    displayDriver->clear();
    
    drawHeader("TEMPERATURE");
    drawThermometerIcon();
    drawCenteredValue(data.temperature, "C", 1);
    drawStatusIndicator(data.temperature, 18.0f, 26.0f); // Ideal range: 18-26°C
    
    displayDriver->update();
}

/*!
 * \brief Draw the humidity page with professional layout
 * \param data Current sensor readings
 */
static void drawHumidity(const SensorData& data) {
    displayDriver->clear();
    
    drawHeader("HUMIDITY");
    drawDropletIcon();
    drawCenteredValue(data.humidity, "%", 1);
    drawStatusIndicator(data.humidity, 40.0f, 70.0f); // Ideal range: 40-70%
    
    displayDriver->update();
}

/*!
 * \brief Draw the soil moisture page with professional layout
 * \param data Current sensor readings
 */
static void drawMoisture(const SensorData& data) {
    displayDriver->clear();
    
    drawHeader("SOIL MOISTURE");
    drawPlantIcon();
    
    // Moisture is typically 0-100 scale or raw sensor value
    // Adjust formatting based on your sensor range
    if (data.moisture > 100.0f) {
        drawCenteredValue(data.moisture, "", 0); // Raw value, no decimals
    } else {
        drawCenteredValue(data.moisture, "%", 0); // Percentage
    }
    
    drawStatusIndicator(data.moisture, 30.0f, 70.0f); // Ideal range: 30-70%
    
    displayDriver->update();
}

/*!
 * \brief Draw the idle face with system status
 */
static void drawFaceIdle() {
    displayDriver->clear();
    
    // Large friendly face (centered)
    displayDriver->setTextSize(4);
    displayDriver->setCursor(40, 40);
    displayDriver->printf(":)");
    
    // Status message
    displayDriver->setTextSize(1);
    int16_t msgX = (128 - 13 * 6) / 2;
    displayDriver->setCursor(msgX, 100);
    displayDriver->printf("Plant is fine");
    
    // Optional: Small icon indicators at top
    displayDriver->setTextSize(1);
    displayDriver->setCursor(4, 4);
    displayDriver->printf("[OK]");
    
    displayDriver->update();
}

/*!
 * \brief Get the next UI state in the FSM sequence
 * \param state Current UI state
 * \return Next UI state
 */
static UiState nextState(UiState state) {
    switch (state) {
        case UiState::FACE_IDLE:        return UiState::PAGE_TEMPERATURE;
        case UiState::PAGE_TEMPERATURE: return UiState::PAGE_HUMIDITY;
        case UiState::PAGE_HUMIDITY:    return UiState::PAGE_MOISTURE;
        case UiState::PAGE_MOISTURE:    return UiState::FACE_IDLE;
        default:                        return UiState::FACE_IDLE;
    }
}

/*!
 * \brief Main display task function
 * \param pvParameters Task parameters (unused)
 */
static void DisplayTask(void*) {
    displayDriver = new DisplayHAL();
    if (!displayDriver->begin()) {
        Serial.println("[DISPLAY] Init failed");
        vTaskDelete(nullptr);
    }

    displayDriver->setTextSize(1);
    displayDriver->setTextColor(COLOR_WHITE);

    currentState = UiState::FACE_IDLE;
    /*if (currentState == UiState::BOOT && !ConfigHandler::isConfigured()){
        currentState = UiState::BLUETOOTH;    
    }*/
    lastInteraction = millis();

    uiEventQueue = xQueueCreate(5, sizeof(uint8_t));

    initButton();

    uint8_t evt;
    SensorData data;

    while (true) {
        uint32_t now = millis();

        if (xQueueReceive(uiEventQueue, &evt, 0) == pdTRUE) {
            if(button.debouncing()){
                currentState = nextState(currentState);
                lastInteraction = now;
            }
        }

        if (currentState != UiState::FACE_IDLE &&
            now - lastInteraction > UI_PAGE_TIMEOUT_MS) {
            currentState = UiState::FACE_IDLE;
        }

        if (now - lastUiUpdate >= UI_UPDATE_INTERVAL_MS) {
            lastUiUpdate = now;
            getLatestSensorData(data);

            switch (currentState) {
                case UiState::FACE_IDLE:        drawFaceIdle(); break;
                case UiState::PAGE_TEMPERATURE: drawTemperature(data); break;
                case UiState::PAGE_HUMIDITY:    drawHumidity(data); break;
                case UiState::PAGE_MOISTURE:    drawMoisture(data); break;
                default: break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void startDisplayTask(uint32_t stackSize, UBaseType_t priority, BaseType_t core) {
    xTaskCreatePinnedToCore(
        DisplayTask,
        "DisplayTask",
        stackSize,
        nullptr,
        priority,
        nullptr,
        core
    );
}

} // namespace Tasks
} // namespace PlantMonitor
