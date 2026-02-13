#include "periodic-timer.h"

namespace PlantMonitor {
namespace Utils {

PeriodicSendTimer::PeriodicSendTimer()
    : m_timer(nullptr)
    , m_period_ms(0)
    , m_running(false)
    , m_due(false)
    , m_count(0)
    , m_mux(portMUX_INITIALIZER_UNLOCKED) {}

PeriodicSendTimer::~PeriodicSendTimer() {
    end();
}

bool PeriodicSendTimer::begin(uint32_t period_ms, bool start_now) {
    end();
    
    m_period_ms = period_ms;
    m_due = false;
    m_count = 0;
    m_running = false;
    
    esp_timer_create_args_t args = {};
    args.callback = &PeriodicSendTimer::timerThunk;
    args.arg = this;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "PeriodicSendTimer";
    
    if (esp_timer_create(&args, &m_timer) != ESP_OK || m_timer == nullptr) {
        m_timer = nullptr;
        return false;
    }
    
    if (start_now) {
        return start();
    }
    return true;
}

bool PeriodicSendTimer::start() {
    if (!m_timer || m_period_ms == 0) return false;
    if (m_running) return true;
    
    const uint64_t us = (uint64_t)m_period_ms * 1000ULL;
    if (esp_timer_start_periodic(m_timer, us) != ESP_OK) {
        return false;
    }
    
    m_running = true;
    return true;
}

void PeriodicSendTimer::stop() {
    if (!m_timer) return;
    if (m_running) {
        esp_timer_stop(m_timer);
        m_running = false;
    }
}

void PeriodicSendTimer::end() {
    stop();
    if (m_timer) {
        esp_timer_delete(m_timer);
        m_timer = nullptr;
    }
}

bool PeriodicSendTimer::isRunning() const {
    return m_running;
}

bool PeriodicSendTimer::take() {
    bool ret = false;
    portENTER_CRITICAL(&m_mux);
    if (m_due) {
        m_due = false;
        ret = true;
    }
    portEXIT_CRITICAL(&m_mux);
    return ret;
}

bool PeriodicSendTimer::peek() const {
    bool ret;
    portENTER_CRITICAL(&m_mux);
    ret = m_due;
    portEXIT_CRITICAL(&m_mux);
    return ret;
}

void PeriodicSendTimer::clear() {
    portENTER_CRITICAL(&m_mux);
    m_due = false;
    portEXIT_CRITICAL(&m_mux);
}

bool PeriodicSendTimer::setPeriodMs(uint32_t period_ms) {
    if (!m_timer || period_ms == 0) return false;
    
    const bool wasRunning = m_running;
    if (wasRunning) stop();
    
    m_period_ms = period_ms;
    
    if (wasRunning) return start();
    return true;
}

uint32_t PeriodicSendTimer::periodMs() const {
    return m_period_ms;
}

uint64_t PeriodicSendTimer::fireCount() const {
    uint64_t c;
    portENTER_CRITICAL(&m_mux);  
    c = m_count;
    portEXIT_CRITICAL(&m_mux);
    return c;
}

void PeriodicSendTimer::timerThunk(void* arg) {
    auto* self = static_cast<PeriodicSendTimer*>(arg);
    if (self) self->onTimer();
}

void PeriodicSendTimer::onTimer() {
    portENTER_CRITICAL(&m_mux);
    m_due = true;
    m_count++;
    portEXIT_CRITICAL(&m_mux);
}

} // namespace Utils
} // namespace PlantMonitor