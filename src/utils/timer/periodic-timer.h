#pragma once
#include <Arduino.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/*!
 * \file periodic-timer.h
 * \brief Thread-safe periodic timer built on top of ESP32 esp_timer.
 *
 * This module provides a reusable periodic timer that signals readiness
 * through a mutex-protected flag, suitable for cooperative polling from
 * FreeRTOS tasks.
 *
 * Typical usage:
 * \code
 * PeriodicSendTimer timer;
 * timer.begin(5000);          // Fire every 5 seconds
 *
 * while (true) {
 *     if (timer.take()) {     // Non-blocking check + consume
 *         // Time to act
 *     }
 *     vTaskDelay(pdMS_TO_TICKS(20));
 * }
 * \endcode
 *
 * \note The timer callback runs in the ESP_TIMER_TASK context (not ISR),
 *       so standard FreeRTOS synchronization primitives are safe to use.
 */

namespace PlantMonitor {
namespace Utils {

/*!
 * \class PeriodicSendTimer
 * \brief Periodic timer with thread-safe signalling.
 *
 * Internally uses \c esp_timer_start_periodic and a binary semaphore to
 * protect the \c m_due flag, which is set by the timer callback and
 * consumed by application code via take().
 */
class PeriodicSendTimer {
  public:
    /*!
     * \brief Construct a timer (not yet started).
     *
     * Creates the internal binary semaphore.
     */
    PeriodicSendTimer();

    /*!
     * \brief Destroy the timer, releasing all resources.
     *
     * Calls end() and deletes the semaphore.
     */
    ~PeriodicSendTimer();

    /*!
     * \brief Create and optionally start the underlying esp_timer.
     * \param period_ms Timer period in milliseconds.
     * \param start_now If true, the timer starts immediately after creation.
     * \return true on success, false if resource allocation failed.
     *
     * Calling begin() on an already-running timer will stop and recreate it.
     */
    bool begin(uint32_t period_ms, bool start_now = true);

    /*!
     * \brief Start the timer with the previously configured period.
     * \return true on success, false if the timer was not created or the period is zero.
     */
    bool start();

    /*!
     * \brief Stop the timer without destroying it.
     *
     * The timer can be restarted later with start().
     */
    void stop();

    /*!
     * \brief Stop and destroy the underlying esp_timer.
     *
     * After this call, begin() must be called again before the timer can be used.
     */
    void end();

    /*!
     * \brief Check whether the timer is currently running.
     * \return true if the timer is active.
     */
    bool isRunning() const;

    /*!
     * \brief Check and consume the pending signal (non-blocking).
     * \return true if the timer had fired since the last take(), false otherwise.
     *
     * This is the primary polling method: it atomically reads and clears
     * the internal \c m_due flag under mutex protection.
     */
    bool take();

    /*!
     * \brief Peek at the pending signal without consuming it.
     * \return true if the timer has fired and the signal has not been consumed.
     */
    bool peek() const;

    /*!
     * \brief Clear the pending signal without consuming it.
     */
    void clear();

    /*!
     * \brief Change the timer period.
     * \param period_ms New period in milliseconds (must be > 0).
     * \return true on success.
     *
     * If the timer is running it will be stopped, reconfigured and restarted.
     */
    bool setPeriodMs(uint32_t period_ms);

    /*!
     * \brief Get the current period.
     * \return Period in milliseconds.
     */
    uint32_t periodMs() const;

    /*!
     * \brief Get the total number of times the timer has fired.
     * \return Fire count (mutex-protected read).
     */
    uint64_t fireCount() const;

  private:
    /// \brief Static trampoline forwarding the esp_timer callback to onTimer().
    static void timerThunk(void *arg);

    /// \brief Timer callback executed in ESP_TIMER_TASK context.
    void onTimer();

  private:
    esp_timer_handle_t m_timer; //!< Underlying esp_timer handle.
    uint32_t m_period_ms;       //!< Configured period (ms).
    bool m_running;             //!< True while the timer is active.

    bool m_due;       //!< Set by onTimer(), cleared by take().
    uint64_t m_count; //!< Cumulative fire count.

    SemaphoreHandle_t m_mutex; //!< Binary semaphore protecting m_due and m_count.
};

} // namespace Utils
} // namespace PlantMonitor
