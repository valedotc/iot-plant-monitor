#pragma once
#include <Arduino.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" 

namespace PlantMonitor {
namespace Utils {

class PeriodicSendTimer {
public:
    PeriodicSendTimer();
    ~PeriodicSendTimer();
    
    bool begin(uint32_t period_ms, bool start_now = true);
    bool start();
    void stop();
    void end();
    
    bool isRunning() const;
    bool take();
    bool peek() const;
    void clear();
    
    bool setPeriodMs(uint32_t period_ms);
    uint32_t periodMs() const;
    uint64_t fireCount() const;

private:
    static void timerThunk(void* arg);
    void onTimer();

private:
    esp_timer_handle_t m_timer;
    uint32_t m_period_ms;
    bool m_running;
    
    bool m_due;
    uint64_t m_count;
    
    SemaphoreHandle_t m_mutex; 
};

} // namespace Utils
} // namespace PlantMonitor