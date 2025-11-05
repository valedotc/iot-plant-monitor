// display_hal.h
#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Wire.h>
#include "app_config.h"

/*!
 * \file display_hal.h
 * \brief Hardware abstraction layer for SH1107 OLED display
 * 
 * This class handles low-level display operations only.
 * High-level UI rendering is managed by ScreenManager.
 * 
 * \note Thread-safe when used with proper mutex in DisplayService
 */

namespace PlantMonitor {
namespace Drivers {

// Costanti per colori monocromatici
constexpr uint16_t COLOR_BLACK = SH110X_BLACK;
constexpr uint16_t COLOR_WHITE = SH110X_WHITE;

/*!
 * \class DisplayHAL
 * \brief Low-level driver for SH1107 128x128 monochrome OLED
 */
class DisplayHAL {
public:
    /*!
     * \brief Constructor
     */
    DisplayHAL();
    
    /*!
     * \brief Destructor
     */
    ~DisplayHAL() = default;
    
    /*!
     * \brief Initialize display hardware
     * \return true if successful, false otherwise
     */
    bool begin();
    
    /*!
     * \brief Clear display buffer (does not update screen)
     */
    void clear();
    
    /*!
     * \brief Flush buffer to physical display
     * \note Call this after drawing operations to make them visible
     */
    void update();
    
    /*!
     * \brief Set display contrast/brightness
     * \param level Brightness level 0-255
     */
    void setBrightness(uint8_t level);
    
    /*!
     * \brief Check if display is initialized
     * \return true if ready, false otherwise
     */
    bool isReady() const { return m_initialized; }
    
    /*!
     * \brief Get display width in pixels
     * \return Width in pixels
     */
    uint8_t getWidth() const { return Config::DISPLAY_WIDTH; }
    
    /*!
     * \brief Get display height in pixels
     * \return Height in pixels
     */
    uint8_t getHeight() const { return Config::DISPLAY_HEIGHT; }
    
    // ============ Drawing Primitives ============
    
    /*!
     * \brief Draw a single pixel
     * \param x X coordinate (0-127)
     * \param y Y coordinate (0-127)
     * \param color Color value (SH110X_WHITE or SH110X_BLACK)
     */
    void drawPixel(int16_t x, int16_t y, uint16_t color);

    /*!
    * \brief Draw a monochrome bitmap
    * \param bitmap Pointer to bitmap data in PROGMEM
    * \param x Top-left X coordinate  
    * \param y Top-left Y coordinate
    * \param w Width in pixels
    * \param h Height in pixels
    * \param color Color value (COLOR_WHITE or COLOR_BLACK)
    */
    void drawBitmap(const uint8_t* bitmap, int16_t x = 0, int16_t y = 0, int16_t w = 128, int16_t h = 128, uint16_t color = COLOR_WHITE);
    
    /*!
     * \brief Draw a line
     * \param x0 Start X coordinate
     * \param y0 Start Y coordinate
     * \param x1 End X coordinate
     * \param y1 End Y coordinate
     * \param color Color value
     */
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    
    /*!
     * \brief Draw a rectangle outline
     * \param x Top-left X
     * \param y Top-left Y
     * \param w Width
     * \param h Height
     * \param color Color value
     */
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    /*!
     * \brief Draw a filled rectangle
     * \param x Top-left X
     * \param y Top-left Y
     * \param w Width
     * \param h Height
     * \param color Color value
     */
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    /*!
     * \brief Draw a circle outline
     * \param x Center X
     * \param y Center Y
     * \param r Radius
     * \param color Color value
     */
    void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    
    /*!
     * \brief Draw a filled circle
     * \param x Center X
     * \param y Center Y
     * \param r Radius
     * \param color Color value
     */
    void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color);
    
    /*!
     * \brief Draw a triangle outline
     */
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);
    
    /*!
     * \brief Draw a filled triangle
     */
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, uint16_t color);
    
    // ============ Text Rendering ============
    
    /*!
     * \brief Set text cursor position
     * \param x X coordinate
     * \param y Y coordinate
     */
    void setCursor(int16_t x, int16_t y);
    
    /*!
     * \brief Set text color
     * \param color Color value (SH110X_WHITE or SH110X_BLACK)
     */
    void setTextColor(uint16_t color);
    
    /*!
     * \brief Set text size multiplier
     * \param size Scale factor (1 = 6x8 pixels per character)
     */
    void setTextSize(uint8_t size);
    
    /*!
     * \brief Print text string at current cursor position
     * \param text Null-terminated string
     */
    void print(const char* text);
    
    /*!
     * \brief Print formatted text (printf-style)
     * \param format Printf format string
     * \param ... Variable arguments
     */
    void printf(const char* format, ...);
    
    /*!
     * \brief Get bounding box of text string
     * \param text Text string to measure
     * \param x Reference X position
     * \param y Reference Y position
     * \param[out] x1 Bounding box left
     * \param[out] y1 Bounding box top
     * \param[out] w Bounding box width
     * \param[out] h Bounding box height
     */
    void getTextBounds(const char* text, int16_t x, int16_t y,
                       int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);
    
private:
    Adafruit_SH1107 m_display;     //!< Adafruit GFX driver instance
    bool m_initialized;              //!< Initialization status flag
    
    // Prevent copying
    DisplayHAL(const DisplayHAL&) = delete;
    DisplayHAL& operator=(const DisplayHAL&) = delete;
};

} // namespace Drivers
} // namespace PlantMonitor