#pragma once
/*!
 * \file esp_timer.h
 * \brief Mock ESP32 timer for native unit testing.
 */

#include <cstdint>

using esp_err_t = int;
#define ESP_OK 0
#define ESP_FAIL -1

struct esp_timer;
using esp_timer_handle_t = esp_timer *;

enum esp_timer_dispatch_t { ESP_TIMER_TASK };

struct esp_timer_create_args_t {
    void (*callback)(void *);
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
};

inline esp_err_t esp_timer_create(const esp_timer_create_args_t *, esp_timer_handle_t *out) {
    *out = reinterpret_cast<esp_timer_handle_t>(1);
    return ESP_OK;
}

inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t, uint64_t) { return ESP_OK; }
inline esp_err_t esp_timer_stop(esp_timer_handle_t) { return ESP_OK; }
inline esp_err_t esp_timer_delete(esp_timer_handle_t) { return ESP_OK; }
