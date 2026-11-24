// LVGL user interface: idle clock, 1-4 adaptive timer rings, settings screen.
// Elements are persistent and animated between layouts by brush_scene.
#pragma once

#include <time.h>

#include "brush_core.h"
#include "brush_i18n.h"

enum ClockFace : uint8_t {
    CLOCK_FACE_CLASSIC = 0,
    CLOCK_FACE_SYNTHWAVE,
    CLOCK_FACE_FLOWER_POWER,
    CLOCK_FACE_CHRISTMAS,
    CLOCK_FACE_NINETIES,
    CLOCK_FACE_REINDEER,
    CLOCK_FACE_HEART,
    CLOCK_FACE_CANDY,
    CLOCK_FACE_COUNT,
};

struct UiSettings {
    bool count_up;
    bool sound;
    brush_lang_t lang;
    ClockFace clock_face;
};

void ui_init(brush_core_t *core, uint32_t now_ms);
// Call from loop(): every frame while ui_animating(), otherwise every ~50 ms.
void ui_update(uint32_t now_ms, const struct tm *local_time, uint16_t millis_in_second,
               const UiSettings &settings, bool screensaver);
bool ui_animating(uint32_t now_ms);
// Visual accent for a timer event (ring "breathes" on 30 s marks, bigger when done).
void ui_timer_event(const brush_event_t &event, uint32_t now_ms);
void ui_open_settings(const struct tm *local_time, const UiSettings &settings);

// Implemented by the application (main.cpp). Called from LVGL event handlers.
void app_start_timer();
void app_timer_tapped(int index);
void app_timer_long_pressed(int index);
void app_open_settings();
void app_change_clock_face(bool next);
void app_settings_done(int hour, int minute, bool time_changed, const UiSettings &settings);
