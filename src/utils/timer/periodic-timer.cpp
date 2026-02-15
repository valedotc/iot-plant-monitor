#include "periodic-timer.h"

namespace PlantMonitor {
namespace Utils {

PeriodicSendTimer::PeriodicSendTimer()
    : m_timer(nullptr)
    , m_period_ms(0)
    , m_running(false)
    , m_due(false)
    , m_count(0)
    , m_mutex(nullptr) {
    
    // ✅ Crea semaforo binario
    m_mutex = xSemaphoreCreateBinary();
    if (m_mutex) {
        xSemaphoreGive(m_mutex);  // Inizializza come "disponibile"
    }
}

PeriodicSendTimer::~PeriodicSendTimer() {
    end();
    
    // ✅ Distruggi semaforo
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

bool PeriodicSendTimer::begin(uint32_t period_ms, bool start_now) {
    end();
    
    m_period_ms = period_ms;
    m_due = false;
    m_count = 0;
    m_running = false;
    
    // ✅ Ricrea semaforo se necessario
    if (!m_mutex) {
        m_mutex = xSemaphoreCreateBinary();
        if (m_mutex) {
            xSemaphoreGive(m_mutex);
        } else {
            return false;  // Fallimento creazione semaforo
        }
    }
    
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
    if (!m_mutex) return false;
    
    bool ret = false;
    
    // ✅ Prendi semaforo (max 10ms di attesa)
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (m_due) {
            m_due = false;
            ret = true;
        }
        xSemaphoreGive(m_mutex);  // ✅ Rilascia semaforo
    }
    
    return ret;
}

bool PeriodicSendTimer::peek() const {
    if (!m_mutex) return false;
    
    bool ret = false;
    
    // ✅ Prendi semaforo
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        ret = m_due;
        xSemaphoreGive(m_mutex);
    }
    
    return ret;
}

void PeriodicSendTimer::clear() {
    if (!m_mutex) return;
    
    // ✅ Prendi semaforo
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        m_due = false;
        xSemaphoreGive(m_mutex);
    }
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
    if (!m_mutex) return 0;
    
    uint64_t c = 0;
    
    // ✅ Prendi semaforo
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        c = m_count;
        xSemaphoreGive(m_mutex);
    }
    
    return c;
}

void PeriodicSendTimer::timerThunk(void* arg) {
    auto* self = static_cast<PeriodicSendTimer*>(arg);
    if (self) self->onTimer();
}

void PeriodicSendTimer::onTimer() {
    if (!m_mutex) return;
    
    // ✅ Usa xSemaphoreTakeFromISR se necessario
    // Ma con ESP_TIMER_TASK (non ISR), possiamo usare normale Take
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // ✅ IMPORTANTE: ESP_TIMER_TASK significa che NON siamo in ISR,
    // quindi possiamo usare xSemaphoreTake normale
    if (xSemaphoreTake(m_mutex, 0) == pdTRUE) {
        m_due = true;
        m_count++;
        xSemaphoreGive(m_mutex);
    }
}

} // namespace Utils
} // namespace PlantMonitor