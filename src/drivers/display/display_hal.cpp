#include "drivers/display/display_hal.h"
#include <cstdarg>

namespace PlantMonitor {
namespace Drivers {

DisplayHAL::DisplayHAL() 
    : m_display(Config::DISPLAY_WIDTH, 
                Config::DISPLAY_HEIGHT, 
                &Wire, 
                Config::DISPLAY_RESET_PIN, 
                1000000UL),
      m_initialized(false) {
}

bool DisplayHAL::begin() {
    // Initialize I2C bus if not already initialized
    Wire.begin(Config::I2C_SDA_PIN, Config::I2C_SCL_PIN);
    Wire.setClock(Config::I2C_FREQUENCY);
    
    // Initialize SSD1327 display
    if (!m_display.begin(Config::DISPLAY_I2C_ADDR)) {
        Serial.println("[DisplayHAL] ERROR: SSD1327 initialization failed!");
        return false;
    }
    
    // Configure display orientation
    m_display.setRotation(0);
    
    // Clear display
    m_display.clearDisplay();
    m_display.display();
    
    m_initialized = true;
    Serial.println("[DisplayHAL] SSD1327 initialized successfully");
    
    return true;
}

void DisplayHAL::clear() {
    m_display.clearDisplay();
}

void DisplayHAL::update() {
    m_display.display();
}

void DisplayHAL::setBrightness(uint8_t level) {
    // Clamp to valid range
    level = constrain(level, 0, 15);
    
    // Note: Actual implementation depends on Adafruit_SSD1327 library support
    // Some implementations might need direct command sending:
    // m_display.ssd1327_command(0x81);  // Set contrast command
    // m_display.ssd1327_command(level << 4);
    
    Serial.printf("[DisplayHAL] Brightness set to %d/15\n", level);
}

// ============ Drawing Primitives ============

void DisplayHAL::drawPixel(int16_t x, int16_t y, uint8_t color) {
    m_display.drawPixel(x, y, color);
}

void DisplayHAL::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    m_display.drawLine(x0, y0, x1, y1, color);
}

void DisplayHAL::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    m_display.drawRect(x, y, w, h, color);
}

void DisplayHAL::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    m_display.fillRect(x, y, w, h, color);
}

void DisplayHAL::drawCircle(int16_t x, int16_t y, int16_t r, uint8_t color) {
    m_display.drawCircle(x, y, r, color);
}

void DisplayHAL::fillCircle(int16_t x, int16_t y, int16_t r, uint8_t color) {
    m_display.fillCircle(x, y, r, color);
}

void DisplayHAL::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              int16_t x2, int16_t y2, uint8_t color) {
    m_display.drawTriangle(x0, y0, x1, y1, x2, y2, color);
}

void DisplayHAL::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                              int16_t x2, int16_t y2, uint8_t color) {
    m_display.fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

// ============ Text Rendering ============

void DisplayHAL::setCursor(int16_t x, int16_t y) {
    m_display.setCursor(x, y);
}

void DisplayHAL::setTextColor(uint8_t color) {
    m_display.setTextColor(color);
}

void DisplayHAL::setTextSize(uint8_t size) {
    m_display.setTextSize(size);
}

void DisplayHAL::print(const char* text) {
    m_display.print(text);
}

void DisplayHAL::printf(const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    m_display.print(buffer);
}

void DisplayHAL::getTextBounds(const char* text, int16_t x, int16_t y,
                               int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
    m_display.getTextBounds(text, x, y, x1, y1, w, h);
}

} // namespace Drivers
} // namespace PlantMonitor