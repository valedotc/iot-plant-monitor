#include "rgb_hal.h"

namespace PlantMonitor {
namespace Drivers {
RGBHal::RGBHal(bool common_anode = false)
    : m_r_pin(Config::RGB_LED_R_PIN),
      m_g_pin(Config::RGB_LED_G_PIN),
      m_b_pin(Config::RGB_LED_B_PIN) {
}

bool RGBHal::begin() {
    pinMode(m_r_pin, OUTPUT);
    pinMode(m_g_pin, OUTPUT);
    pinMode(m_b_pin, OUTPUT);

    // Turn off LED initially
    turnOff();

    Serial.println("[RGBHal] RGB LED initialized successfully");
    return true;
}

void RGBHal::setColor(uint8_t r, uint8_t g, uint8_t b) {
    analogWrite(m_r_pin, r);
    analogWrite(m_g_pin, g);
    analogWrite(m_b_pin, b);
}

void RGBHal::turnOff() {
    analogWrite(m_r_pin, 0);
    analogWrite(m_g_pin, 0);
    analogWrite(m_b_pin, 0);
}

} // namespace Drivers
} // namespace PlantMonitor