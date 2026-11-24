#include "board/buzzer.h"

#include "board/exio.h"
#include "board/hal.h"
#include "board/pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace {

// Alternating on/off durations in ms, starting with "on", terminated by 0.
// Mirrored in preview/index.html.
const uint16_t kStart[] = {45, 0};
const uint16_t kMark1[] = {30, 0};
const uint16_t kMark2[] = {30, 65, 30, 0};
const uint16_t kMark3[] = {30, 65, 30, 65, 30, 0};
const uint16_t kDone[] = {45, 95, 75, 0};

constexpr uint16_t GAP_BETWEEN_PATTERNS_MS = 120;
constexpr uint32_t DEDUPE_WINDOW_MS = 150;

const uint16_t *pattern_for(Beep b)
{
    switch (b) {
    case Beep::Start: return kStart;
    case Beep::Mark1: return kMark1;
    case Beep::Mark2: return kMark2;
    case Beep::Mark3: return kMark3;
    case Beep::Done: return kDone;
    }
    return nullptr;
}

QueueHandle_t s_queue = nullptr;
volatile bool s_enabled = true;

void buzzer_task(void *)
{
    Beep beep;
    for (;;) {
        if (xQueueReceive(s_queue, &beep, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const uint16_t *p = pattern_for(beep);
        if (!p) {
            ESP_LOGW("buzzer", "unknown cue: %u", static_cast<unsigned>(beep));
            continue;
        }
        for (int step = 0; p[step] != 0 && s_enabled; step++) {
            exio_write(EXIO_BUZZER, (step % 2) == 0);
            vTaskDelay(pdMS_TO_TICKS(p[step]));
        }
        exio_write(EXIO_BUZZER, false);
        vTaskDelay(pdMS_TO_TICKS(GAP_BETWEEN_PATTERNS_MS));
    }
}

} // namespace

Beep buzzer_mark(uint8_t n)
{
    switch (n) {
    case 1: return Beep::Mark1;
    case 2: return Beep::Mark2;
    default: return Beep::Mark3;
    }
}

void buzzer_init()
{
    exio_write(EXIO_BUZZER, false);
    s_queue = xQueueCreate(4, sizeof(Beep));
    // Core 0, above the UI task priority, so patterns are not stretched by rendering.
    xTaskCreatePinnedToCore(buzzer_task, "buzzer", 2048, nullptr, 5, nullptr, 0);
}

void buzzer_enable(bool enabled) { s_enabled = enabled; }

void buzzer_play(Beep beep)
{
    if (!s_enabled || !s_queue) {
        return;
    }
    // Several timers hitting the same mark at once should sound once.
    const uint32_t now = millis();
    static Beep last = beep;
    static uint32_t last_ms = now - DEDUPE_WINDOW_MS;
    if (beep == last && now - last_ms < DEDUPE_WINDOW_MS) {
        return;
    }
    last = beep;
    last_ms = now;
    xQueueSend(s_queue, &beep, 0);
}
