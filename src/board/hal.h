// Small replacements for the Arduino time helpers.
#pragma once

#include <stdint.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Milliseconds since boot; wraps after ~49 days like Arduino's millis() (all users handle that).
inline uint32_t millis() { return (uint32_t)(esp_timer_get_time() / 1000); }

inline void delay_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms) > 0 ? pdMS_TO_TICKS(ms) : 1); }
