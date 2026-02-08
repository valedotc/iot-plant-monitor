// display-hal.cpp
#include "drivers/display/display-hal.h"
#include <cstdarg>

namespace PlantMonitor {
namespace Drivers {

DisplayHAL::DisplayHAL()
    : m_display(Config::DISPLAY_WIDTH,
                Config::DISPLAY_HEIGHT,
                &Wire,
                Config::DISPLAY_RESET_PIN),
      m_initialized(false) {
}

bool DisplayHAL::begin() {
    Wire.begin();
    Wire.setClock(Config::I2C_FREQUENCY);

    // IMPORTANT: pass true as second parameter for reset
    if (!m_display.begin(Config::DISPLAY_I2C_ADDR, true)) {
        Serial.println("[DisplayHAL] ERROR: SH1107 initialization failed!");
        return false;
    }

    m_display.setRotation(0);
    m_display.clearDisplay();
    m_display.display();

    m_initialized = true;
    Serial.println("[DisplayHAL] SH1107 initialized successfully");

    return true;
}

void DisplayHAL::clear() {
    m_display.clearDisplay();
}

void DisplayHAL::update() {
    m_display.display();
}

void DisplayHAL::setBrightness(uint8_t level) {
    // SH1107 supports contrast 0-255
    m_display.setContrast(level);
    Serial.printf("[DisplayHAL] Contrast set to %d/255\n", level);
}

// ============ Drawing Primitives ============

void DisplayHAL::drawPixel(int16_t x, int16_t y, uint16_t color) {
    m_display.drawPixel(x, y, color);
}

void DisplayHAL::drawBitmap(const uint8_t *bitmap, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    m_display.drawBitmap(x, y, bitmap, w, h, color);
}

void DisplayHAL::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    m_display.drawLine(x0, y0, x1, y1, color);
}

void DisplayHAL::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    m_display.drawRect(x, y, w, h, color);
}

void DisplayHAL::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    m_display.fillRect(x, y, w, h, color);
}

void DisplayHAL::drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    m_display.drawCircle(x, y, r, color);
}

void DisplayHAL::fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    m_display.fillCircle(x, y, r, color);
}

void DisplayHAL::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    m_display.drawTriangle(x0, y0, x1, y1, x2, y2, color);
}

void DisplayHAL::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color) {
    m_display.fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

// ============ Text Rendering ============

void DisplayHAL::setCursor(int16_t x, int16_t y) {
    m_display.setCursor(x, y);
}

void DisplayHAL::setTextColor(uint16_t color) {
    m_display.setTextColor(color);
}

void DisplayHAL::setTextSize(uint8_t size) {
    m_display.setTextSize(size);
}

void DisplayHAL::print(const char *text) {
    m_display.print(text);
}

void DisplayHAL::printf(const char *format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    m_display.print(buffer);
}

void DisplayHAL::getTextBounds(const char *text, int16_t x, int16_t y, int16_t *x1, int16_t *y1, uint16_t *w, uint16_t *h) {
    m_display.getTextBounds(text, x, y, x1, y1, w, h);
}

} // namespace Drivers
} // namespace PlantMonitor