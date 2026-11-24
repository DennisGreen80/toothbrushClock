// Toothbrush timer for the Waveshare ESP32-S3-Touch-LCD-2.8C.
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <lvgl.h>
#include <sys/time.h>
#include <time.h>

#include "board/buzzer.h"
#include "board/display.h"
#include "board/exio.h"
#include "board/oralb_scanner.h"
#include "board/pins.h"
#include "board/rtc_pcf85063.h"
#include "board/touch.h"
#include "brush_core.h"
#include "brush_oralb.h"
#include "ui/ui.h"

namespace {

// ---- Behaviour ---------------------------------------------------------------------------
constexpr uint32_t DEFAULT_DURATION_S = 120; // brushing time until changed in settings (120/100/60/40 s)
constexpr uint32_t AUTO_CLEAR_MS = 6 * 1000;  // finished timers (star smiley) disappear 6 s after 2:00
constexpr uint32_t FINISH_HOLD_MS = 6 * 1000;  // a cancelled timer shows its smiley this long
constexpr brush_lang_t DEFAULT_LANG = BRUSH_LANG_DA; // until changed in settings

constexpr uint8_t BRIGHTNESS_ACTIVE = 90;
constexpr uint8_t BRIGHTNESS_IDLE = 25;
constexpr uint8_t BRIGHTNESS_SCREENSAVER = 8;
constexpr uint32_t DIM_AFTER_MS = 3 * 60 * 1000;
constexpr uint32_t SCREENSAVER_AFTER_MS = 15 * 60 * 1000;
constexpr uint32_t BACKLIGHT_STEP_MS = 8; // per 1 % brightness change -> smooth fades

constexpr uint32_t UI_UPDATE_MS = 50;
constexpr uint16_t LONG_PRESS_MS = 400; // hold time for settings / cancelling a timer
constexpr uint32_t RTC_RESYNC_MS = 10 * 60 * 1000;
constexpr int DRAW_BUF_LINES = 40;

// ---- State -------------------------------------------------------------------------------
brush_core_t s_core;
oralb_tracker_t s_oralb;
Preferences s_prefs;
UiSettings s_settings = {false, true, DEFAULT_LANG, CLOCK_FACE_CLASSIC, 100, true, DEFAULT_DURATION_S};

uint32_t s_last_activity_ms = 0;
uint8_t s_brightness = 0;
uint32_t s_backlight_step_ms = 0;

lv_disp_draw_buf_t s_draw_buf;
lv_disp_drv_t s_disp_drv;
lv_indev_drv_t s_indev_drv;

// ---- Time --------------------------------------------------------------------------------
void set_system_time(const struct tm &local)
{
    struct tm copy = local;
    struct timeval tv = {mktime(&copy), 0};
    settimeofday(&tv, nullptr);
}

void get_local_time(struct tm *out, uint16_t *millis_in_second)
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t secs = tv.tv_sec;
    localtime_r(&secs, out);
    if (millis_in_second) {
        *millis_in_second = (uint16_t)(tv.tv_usec / 1000);
    }
}

// Firmware build time, used to seed an RTC that lost power.
void build_time(struct tm *out)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char mon[4] = {0};
    int day = 1, year = 2025, hh = 0, mm = 0, ss = 0;
    sscanf(__DATE__, "%3s %d %d", mon, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hh, &mm, &ss);
    memset(out, 0, sizeof(*out));
    out->tm_year = year - 1900;
    out->tm_mon = (int)((strstr(months, mon) - months) / 3);
    out->tm_mday = day;
    out->tm_hour = hh;
    out->tm_min = mm;
    out->tm_sec = ss;
    out->tm_isdst = -1;
    mktime(out); // fills in tm_wday
}

void init_time()
{
    struct tm t;
    bool have_rtc = rtc_init();
    if (have_rtc && rtc_read(&t) && t.tm_year + 1900 >= 2024) {
        set_system_time(t);
        Serial.printf("[time] RTC %02d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
        return;
    }
    build_time(&t);
    set_system_time(t);
    if (have_rtc) {
        rtc_write(&t);
    }
    Serial.println("[time] RTC invalid, seeded from build time");
}

void resync_from_rtc()
{
    struct tm t;
    if (rtc_read(&t) && t.tm_year + 1900 >= 2024) {
        set_system_time(t);
    }
}

// ---- LVGL glue ---------------------------------------------------------------------------
void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *pixels)
{
    display_flush(area->x1, area->y1, area->x2, area->y2, pixels);
    lv_disp_flush_ready(drv);
}

bool screensaver_active(uint32_t now_ms);

// A touch that starts while the screensaver is on only wakes the screen: it is hidden from LVGL
// until the finger lifts, so starting a timer needs a second tap.
bool s_touch_down = false, s_swallow_touch = false;

void lvgl_touch_cb(lv_indev_drv_t *, lv_indev_data_t *data)
{
    uint16_t x = 0, y = 0;
    if (touch_read(&x, &y)) {
        const uint32_t now = millis();
        if (!s_touch_down && screensaver_active(now)) {
            s_swallow_touch = true;
        }
        s_touch_down = true;
        s_last_activity_ms = now;
    } else {
        s_touch_down = s_swallow_touch = false;
    }
    if (s_touch_down && !s_swallow_touch) {
        data->point.x = (lv_coord_t)x;
        data->point.y = (lv_coord_t)y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void *alloc_draw_buffer(size_t bytes)
{
    void *p = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    return p ? p : heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
}

void init_lvgl()
{
    lv_init();

    const size_t px = LCD_WIDTH * DRAW_BUF_LINES;
    void *buf1 = alloc_draw_buffer(px * sizeof(lv_color_t));
    void *buf2 = alloc_draw_buffer(px * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&s_draw_buf, buf1, buf2, px);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = LCD_WIDTH;
    s_disp_drv.ver_res = LCD_HEIGHT;
    s_disp_drv.flush_cb = lvgl_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = lvgl_touch_cb;
    s_indev_drv.long_press_time = LONG_PRESS_MS;
    lv_indev_drv_register(&s_indev_drv);
}

// ---- Behaviour helpers ----------------------------------------------------------------------
void load_settings()
{
    s_prefs.begin("brush", false);
    s_settings.count_up = s_prefs.getBool("count_up", false);
    s_settings.sound = s_prefs.getBool("sound", true);
    uint8_t lang = s_prefs.getUChar("lang", DEFAULT_LANG);
    s_settings.lang = lang < BRUSH_LANG_COUNT ? (brush_lang_t)lang : DEFAULT_LANG;
    const uint8_t face = s_prefs.getUChar("clock_face", CLOCK_FACE_CLASSIC);
    s_settings.clock_face = face < CLOCK_FACE_COUNT ? static_cast<ClockFace>(face) : CLOCK_FACE_CLASSIC;
    const uint8_t brightness = s_prefs.getUChar("brightness", 100);
    s_settings.brightness = brightness >= 25 && brightness <= 100 ? brightness : 100;
    s_settings.custom_font = s_prefs.getBool("custom_font", true);
    const uint16_t duration = s_prefs.getUShort("duration", DEFAULT_DURATION_S);
    s_settings.duration_s = duration >= 20 && duration <= 600 ? duration : DEFAULT_DURATION_S;
}

void save_settings()
{
    s_prefs.putBool("count_up", s_settings.count_up);
    s_prefs.putBool("sound", s_settings.sound);
    s_prefs.putUChar("lang", (uint8_t)s_settings.lang);
    s_prefs.putUChar("clock_face", s_settings.clock_face);
    s_prefs.putUChar("brightness", s_settings.brightness);
    s_prefs.putBool("custom_font", s_settings.custom_font);
    s_prefs.putUShort("duration", s_settings.duration_s);
}

// Eases the backlight towards its target 1 % at a time (smooth wake/dim, fade-in on boot).
bool screensaver_active(uint32_t now_ms)
{
    return s_core.count == 0 && now_ms - s_last_activity_ms >= SCREENSAVER_AFTER_MS;
}

uint8_t backlight_level(uint32_t now_ms)
{
    if (s_core.count != 0) {
        return BRIGHTNESS_ACTIVE;
    }
    const uint32_t idle_ms = now_ms - s_last_activity_ms;
    if (idle_ms >= SCREENSAVER_AFTER_MS) {
        return BRIGHTNESS_SCREENSAVER;
    }
    return idle_ms >= DIM_AFTER_MS ? BRIGHTNESS_IDLE : BRIGHTNESS_ACTIVE;
}

// The brightness setting scales every level, so dimming and the screensaver keep their proportions.
uint8_t backlight_target(uint32_t now_ms)
{
    return (uint8_t)LV_MAX(1, backlight_level(now_ms) * s_settings.brightness / 100);
}

void update_backlight(uint32_t now_ms)
{
    const uint8_t target = backlight_target(now_ms);
    if (target == s_brightness || now_ms - s_backlight_step_ms < BACKLIGHT_STEP_MS) {
        return;
    }
    s_backlight_step_ms = now_ms;
    s_brightness += target > s_brightness ? 1 : -1;
    backlight_set(s_brightness);
}

bool backlight_fading()
{
    return s_brightness != backlight_target(millis());
}

void handle_core_events(uint32_t now_ms)
{
    brush_event_t events[BRUSH_MAX_TIMERS * 2];
    int n = brush_tick(&s_core, now_ms, events, (int)(sizeof(events) / sizeof(events[0])));
    for (int i = 0; i < n; i++) {
        switch (events[i].type) {
        case BEV_MARK: buzzer_play(buzzer_mark(events[i].mark)); break;
        case BEV_DONE: buzzer_play(Beep::Done); break;
        case BEV_CLEARED: break;
        }
        ui_timer_event(events[i], now_ms);
    }
}

void handle_oralb_action(oralb_action_t action, uint32_t now_ms)
{
    switch (action) {
    case ORALB_STARTED:
        buzzer_play(Beep::Start);
        s_last_activity_ms = now_ms; // wake the screen
        break;
    case ORALB_RESUMED:
        s_last_activity_ms = now_ms;
        break;
    case ORALB_FULL: // all four slots busy; nothing sensible to show
    case ORALB_PAUSED:
    case ORALB_ENDED:
    case ORALB_NONE:
        break;
    }
}

void handle_oralb(uint32_t now_ms)
{
    OralbAdvertisement item;
    while (oralb_scanner_poll(&item)) {
        handle_oralb_action(brush_oralb_on_adv(&s_oralb, &s_core, item.addr, &item.adv, now_ms), now_ms);
    }
    handle_oralb_action(brush_oralb_tick(&s_oralb, &s_core, now_ms), now_ms);
}

} // namespace

// ---- Actions from the UI ----------------------------------------------------------------------
void app_start_timer()
{
    if (brush_add(&s_core, millis()) >= 0) {
        buzzer_play(Beep::Start);
    }
}

void app_timer_tapped(int index)
{
    if (index >= s_core.count) {
        return;
    }
    if (s_core.timers[index].state == BT_DONE || s_core.timers[index].finishing) {
        brush_remove(&s_core, index);
    } else if (s_core.timers[index].source == BRUSH_SRC_ORALB) {
        return; // the toothbrush's power button pauses/resumes these
    } else {
        brush_toggle_pause(&s_core, index, millis());
    }
}

void app_timer_long_pressed(int index)
{
    brush_finish(&s_core, index, millis());
}

void app_open_settings()
{
    struct tm now;
    get_local_time(&now, nullptr);
    ui_open_settings(&now, s_settings);
}

void app_change_clock_face(bool next)
{
    const int step = next ? 1 : CLOCK_FACE_COUNT - 1;
    s_settings.clock_face = static_cast<ClockFace>((s_settings.clock_face + step) % CLOCK_FACE_COUNT);
    save_settings();
}

void app_preview_brightness(uint8_t percent)
{
    s_settings.brightness = percent; // saved together with the rest when settings close
}

void app_settings_done(int hour, int minute, bool time_changed, const UiSettings &settings)
{
    if (time_changed) {
        struct tm t;
        get_local_time(&t, nullptr);
        t.tm_hour = hour;
        t.tm_min = minute;
        t.tm_sec = 0;
        t.tm_isdst = -1;
        mktime(&t);
        set_system_time(t);
        rtc_write(&t);
    }
    s_settings = settings;
    brush_set_duration(&s_core, s_settings.duration_s * 1000u);
    save_settings();
    buzzer_enable(s_settings.sound);
}

// ---- Arduino entry points -----------------------------------------------------------------------
void setup()
{
    Serial.begin(115200);
    Serial.println("\n[toothbrush] starting");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
    exio_init();
    backlight_init();

    if (!display_init()) {
        Serial.println("[toothbrush] display init failed");
    }
    touch_init();
    init_time();
    load_settings();
    buzzer_init();
    buzzer_enable(s_settings.sound);

    brush_cfg_t cfg = {s_settings.duration_s * 1000u, 0, AUTO_CLEAR_MS, FINISH_HOLD_MS}; // marks: quarters
    brush_init(&s_core, &cfg);

    init_lvgl();
    ui_init(&s_core, millis());

    // Render the first frame before the backlight fades in to avoid a flash of garbage.
    struct tm now;
    uint16_t ms = 0;
    get_local_time(&now, &ms);
    ui_update(millis(), &now, ms, s_settings, false);
    lv_timer_handler();

    s_last_activity_ms = millis();
    brush_oralb_init(&s_oralb);
    oralb_scanner_start();
}

void loop()
{
    static uint32_t last_ui_ms = 0;
    static uint32_t last_resync_ms = 0;
    const uint32_t now = millis();

    handle_oralb(now);
    handle_core_events(now);
    if (s_core.count == 0) {
        s_core.first_color = (uint8_t)(esp_random() % BRUSH_MAX_TIMERS); // first timer gets a random color
    }

    // Every loop while something moves (smooth animations), otherwise ~20 Hz.
    const bool animating = ui_animating(now);
    if (animating || now - last_ui_ms >= UI_UPDATE_MS) {
        last_ui_ms = now;
        struct tm local;
        uint16_t ms = 0;
        get_local_time(&local, &ms);
        ui_update(now, &local, ms, s_settings, screensaver_active(now));
    }
    update_backlight(now);

    if (now - last_resync_ms >= RTC_RESYNC_MS) {
        last_resync_ms = now;
        resync_from_rtc();
    }

    lv_timer_handler();
    delay(animating || backlight_fading() ? 1 : 5);
}
