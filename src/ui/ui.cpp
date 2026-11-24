#include "ui/ui.h"

#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "board/pins.h"
#include "brush_scene.h"
#include "ui/brush_icon.h"
#include "ui/seg_digits.h"

namespace {

// ---- Look & feel (mirrored in preview/sim.js) ----------------------------------------
const uint32_t kTimerColors[BRUSH_MAX_TIMERS] = {0x3DD6F5, 0xFF7AB6, 0xFFC24B, 0x9B8CFF};
constexpr uint32_t COLOR_TEXT = 0xECECEC;
constexpr uint32_t COLOR_MUTED = 0x8E8E93;
constexpr uint32_t COLOR_FAINT = 0x5A5A5E;
constexpr uint32_t COLOR_SURFACE = 0x1C1C1E;
constexpr uint32_t COLOR_SURFACE_PRESSED = 0x2C2C2E;
constexpr uint32_t COLOR_OUTLINE = 0x3A3A3C;
constexpr uint32_t COLOR_PRESSURE_OK = 0xFFFFFF;
constexpr uint32_t COLOR_PRESSURE_HIGH = 0xFF453A;
constexpr lv_opa_t PRESSURE_OK_OPA = 90;
constexpr lv_opa_t TRACK_OPA = 40;
constexpr lv_coord_t DATE_GAP = 34;
constexpr uint32_t SYNTH_CLOCK = 0xFFD4F7;
constexpr uint32_t SYNTH_DATE = 0xA4F5FF;
constexpr uint32_t SYNTH_HINT = 0xD6B2F2;
constexpr uint32_t SYNTH_TRACK = 0x321841;
constexpr lv_coord_t SYNTH_HINT_TOP = 342;
constexpr lv_coord_t SYNTH_HORIZON = 366;
constexpr uint32_t SYNTH_GRID_CYCLE_MS = 24000;
constexpr uint32_t kSynthGridColors[4] = {0xA43983, 0x7051B1, 0x368DA2, 0x6357AD};
constexpr uint32_t kSynthRingColors[4] = {0xFF53BB, 0xA685FF, 0x54E8F7, 0xFFAB6B};
constexpr uint32_t FLOWER_INK = 0x43223B;
constexpr uint32_t FLOWER_PAPER = 0xFFE9BA;
constexpr uint32_t FLOWER_CORAL = 0xE66A4E;
constexpr uint32_t FLOWER_PINK = 0xBA435F;
constexpr uint32_t FLOWER_GOLD = 0xF5B640;
constexpr uint32_t FLOWER_GREEN = 0x527763;
constexpr uint32_t kFlowerRingColors[] = {0xFFC557, 0xF78E60, 0xF48CA5, 0x9CC18E};
constexpr lv_coord_t FLOWER_HINT_TOP = 348;
constexpr int FLOWER_MARKS = 60;
constexpr uint32_t CHRISTMAS_GOLD = 0xFFD58D;
constexpr uint32_t CHRISTMAS_SNOW = 0xFFF9E8;
constexpr uint32_t CHRISTMAS_TRACK = 0x29443A;
constexpr lv_coord_t CHRISTMAS_HINT_TOP = 344;
constexpr uint32_t CANDY_RED = 0xD7263D, CANDY_WHITE = 0xFFFFFF;

// Per-face text and seconds-ring colors, indexed by ClockFace (mirrored in preview/sim.js).
// `bg`/`rim` are the simplified dial shown behind the timer rings (0 = plain black). Faces with
// a light dial set `ink`: timer colors are darkened and status text uses `ink` so they stay readable.
enum RimStyle : uint8_t { RIM_PLAIN, RIM_STITCH, RIM_CANDY, RIM_FLOWER };
struct FacePalette {
    uint32_t clock, date, hint, track, ring;
    lv_coord_t hint_top;
    uint32_t bg, rim;
    RimStyle rim_style;
    uint32_t ink;
};
const FacePalette kFacePalettes[CLOCK_FACE_COUNT] = {
    {COLOR_TEXT, COLOR_MUTED, COLOR_FAINT, 0x1A1A1C, 0x3DD6F5, 372, 0, 0, RIM_PLAIN, 0},                         // classic
    {SYNTH_CLOCK, SYNTH_DATE, SYNTH_HINT, SYNTH_TRACK, 0x3DD6F5, SYNTH_HINT_TOP, 0x241040, 0xFF53BB, RIM_PLAIN, 0}, // synthwave
    {FLOWER_INK, 0x6C304A, 0x3D5C4E, FLOWER_INK, 0x3DD6F5, FLOWER_HINT_TOP, FLOWER_PAPER, FLOWER_INK, RIM_FLOWER,
     FLOWER_INK},                                                                                                // flower power
    {CHRISTMAS_SNOW, CHRISTMAS_GOLD, 0xA6C3B4, CHRISTMAS_TRACK, CHRISTMAS_GOLD, CHRISTMAS_HINT_TOP, 0x112A25, 0x36554A,
     RIM_PLAIN, 0},
    {0xFFF2B0, 0x63EBD0, 0xEFA7DB, 0x473466, 0xFF69B4, CHRISTMAS_HINT_TOP, 0x211638, 0x7A59C9, RIM_PLAIN, 0},       // 90s
    {0xF1FAFF, 0xA9D9F0, 0xA9C6D9, 0x36536A, 0xD9F4FF, CHRISTMAS_HINT_TOP, 0x102B43, 0x527897, RIM_PLAIN, 0},       // reindeer
    {0xF4EBD7, 0xE9C99A, 0x9DBFA6, 0x2E5E43, 0xD93A3A, CHRISTMAS_HINT_TOP, 0x163A2A, 0x0E2A1D, RIM_STITCH, 0},      // woven heart
    {0xB3122E, 0x2E8B57, 0xC46B7D, 0xF7C5CF, CANDY_RED, CHRISTMAS_HINT_TOP, 0xFFF3E8, CANDY_RED, RIM_CANDY,
     0x8A2A3A},                                                                                                  // candy cane
};
const FacePalette *s_palette = &kFacePalettes[CLOCK_FACE_CLASSIC]; // current face

// Optional per-face fonts for the big idle clock and the date (nullptr = segment digits / Montserrat).
struct FaceFonts {
    const lv_font_t *clock, *date;
};

FaceFonts face_fonts(ClockFace face, bool enabled)
{
    if (enabled) {
        switch (face) {
        case CLOCK_FACE_CHRISTMAS:
        case CLOCK_FACE_REINDEER:
        case CLOCK_FACE_HEART: return {&tb_font_berkshire_clock, &tb_font_berkshire_28};
        case CLOCK_FACE_CANDY: return {&tb_font_fredoka_clock, &tb_font_fredoka_28};
        default: break;
        }
    }
    return {nullptr, nullptr};
}

// Timer colors on light dials are darkened by 30 % to keep contrast.
uint32_t card_color(uint32_t color)
{
    if (!s_palette->ink) {
        return color;
    }
    const auto dark = [&](int shift) { return (uint32_t)(((color >> shift) & 0xFF) * 7 / 10) << shift; };
    return dark(16) | dark(8) | dark(0);
}
constexpr lv_coord_t GEAR_CX = 432, GEAR_CY = 240, GEAR_SIZE = 44;
constexpr uint32_t SCREENSAVER_TRANSITION_MS = 1400;
constexpr uint32_t SCREENSAVER_COLOR_CYCLE_MS = 64000;
constexpr uint32_t SCREENSAVER_COLOR_SEGMENT_MS = SCREENSAVER_COLOR_CYCLE_MS / BRUSH_MAX_TIMERS;
constexpr uint32_t SCREENSAVER_DRIFT_DELAY_MS = 5 * 60 * 1000;
constexpr uint32_t SCREENSAVER_POSITION_CYCLE_MS = 90000;
constexpr float SCREENSAVER_SHIFT = 8.0f;

struct Birthday {
    int month;
    int day;
};

constexpr std::initializer_list<Birthday> kBirthdays = {
    // Add recurring dates as {month, day}. For example: {6, 14},
};

constexpr uint32_t FLAG_FRAME_MS = 50;
constexpr uint32_t FLAG_WAVE_MS = 2400;
constexpr lv_coord_t FLAG_BOX_WIDTH = 82;
constexpr lv_coord_t FLAG_CLOTH_WIDTH = 74;

struct BirthdayFlag {
    lv_obj_t *obj;
    lv_coord_t x;
    uint32_t phase_offset_ms;
    uint32_t phase_ms;
    lv_opa_t opa;
};

BirthdayFlag s_flags[] = {{nullptr, 96, 0, 0, 0},
                          {nullptr, 302, FLAG_WAVE_MS / 3, 0, 0}};

// Ring "breathing" accents (permille of radius, duration ms)
constexpr uint16_t PULSE_MARK_AMP = 30, PULSE_MARK_MS = 380;
constexpr uint16_t PULSE_DONE_AMP = 80, PULSE_DONE_MS = 650;
constexpr uint16_t PULSE_PRESSURE_AMP = 22, PULSE_PRESSURE_MS = 300;

// Finish smiley (mirrors preview/smileys.html). Times are ms after the timer finished.
constexpr uint32_t SMILEY_GOLD = 0xFFD58D;
constexpr uint32_t SMILEY_CONTENT_OUT_MS = 350;               // digits and status fade out
constexpr uint32_t SMILEY_MORPH_DELAY_MS = 200, SMILEY_MORPH_MS = 700; // ring contracts into the head
constexpr uint32_t SMILEY_EYE_MS[2] = {1000, 1100}, SMILEY_EYE_POP_MS = 400;
constexpr uint32_t SMILEY_MOUTH_MS = 1300, SMILEY_MOUTH_DRAW_MS = 450;
constexpr uint32_t SMILEY_BOUNCE_MS = 1800, SMILEY_BOUNCE_DUR_MS = 500;
constexpr uint32_t SMILEY_INTRO_MS = SMILEY_BOUNCE_MS + SMILEY_BOUNCE_DUR_MS;
constexpr uint32_t SMILEY_LOOP_MS = 1600; // star twinkle and sparkle cycle
constexpr float SMILEY_UNIT = 213.0f;     // preview ring outer radius; face geometry is in these units

const brush_strings_t *s_txt = brush_strings(BRUSH_LANG_EN); // active language

// One ring per timer color slot; persistent so it can animate between layouts.
struct Card {
    lv_obj_t *obj;
    lv_obj_t *arcs[BRUSH_QUADRANTS];
    lv_obj_t *digits;
    lv_obj_t *status;
    lv_obj_t *icon;     // toothbrush, Oral-B timers only
    lv_obj_t *pressure; // inner ring, Oral-B timers only
    lv_obj_t *face;     // finish smiley, drawn by draw_smiley()
    lv_coord_t cx, cy, r; // last applied geometry
    lv_coord_t arc_w;
    int half_gap;
    lv_opa_t opa;
    lv_opa_t content_opa; // digits/status/icon, faded out when the smiley appears
    int8_t state;   // last shown timer state (kept while fading out)
    uint32_t color; // last shown color
    bool oralb;     // last shown source
    bool pressure_high;
    bool smiley;
    uint8_t rating;          // brush_rating() when the smiley appeared
    uint16_t quad_fill[BRUSH_QUADRANTS]; // ring fill when the smiley appeared
    uint32_t smiley_ms, smiley_now;
};

brush_core_t *s_core = nullptr;
brush_scene_t s_scene;
lv_obj_t *s_main_scr = nullptr;
lv_obj_t *s_view = nullptr;

lv_obj_t *s_synth_backdrop = nullptr;
lv_opa_t s_synth_opa = LV_OPA_TRANSP;
lv_color_t s_synth_grid;
lv_obj_t *s_flower_backdrop = nullptr;
lv_opa_t s_flower_opa = LV_OPA_TRANSP;
lv_opa_t s_flower_ring_opa = LV_OPA_TRANSP;
int s_flower_second = 0;
lv_point_t s_flower_marks[FLOWER_MARKS];
lv_obj_t *s_christmas_backdrop = nullptr;
lv_opa_t s_christmas_opa = LV_OPA_TRANSP;
lv_obj_t *s_extra_backdrop = nullptr;
lv_opa_t s_extra_opa = LV_OPA_TRANSP;
ClockFace s_extra_face = CLOCK_FACE_NINETIES;
lv_opa_t s_candy_opa = LV_OPA_TRANSP; // candy-cane striped seconds ring
lv_obj_t *s_timer_bg = nullptr;       // simplified face dial behind the timer rings
lv_opa_t s_timer_bg_opa = LV_OPA_TRANSP;
ClockFace s_timer_bg_face = CLOCK_FACE_CLASSIC;
ClockFace s_hint_face = CLOCK_FACE_CLASSIC;
lv_obj_t *s_clock = nullptr;
lv_obj_t *s_font_clock = nullptr;
const lv_font_t *s_font_clock_font = nullptr;
lv_coord_t s_font_clock_x = -1, s_font_clock_y = -1;
lv_coord_t s_clock_h = -1, s_clock_top = -1;
lv_obj_t *s_seconds_arc = nullptr;
lv_obj_t *s_date = nullptr;
lv_obj_t *s_hint = nullptr;
lv_obj_t *s_gear = nullptr;
lv_obj_t *s_gear_icon = nullptr;
lv_opa_t s_idle_opa = 0;
char s_date_text[40] = "";
lv_coord_t s_clock_x = -1, s_clock_y = -1;
lv_coord_t s_date_x = -1, s_date_y = -1;
lv_coord_t s_hint_x = -1, s_hint_y = -1;
float s_saver_level = 0.0f, s_saver_from = 0.0f;
bool s_saver_target = false, s_saver_transitioning = false;
uint32_t s_saver_start_ms = 0, s_saver_epoch_ms = 0;

lv_obj_t *s_add = nullptr;
lv_obj_t *s_add_plus = nullptr;
bool s_add_ghost = false;
lv_coord_t s_add_cx = -1, s_add_cy = -1, s_add_r = -1;
lv_opa_t s_add_opa = 1;

Card s_cards[BRUSH_MAX_TIMERS];

// Settings screen
lv_obj_t *s_settings_scr = nullptr;
lv_obj_t *s_set_digits = nullptr;
lv_obj_t *s_mode_label = nullptr;
lv_obj_t *s_sound_label = nullptr;
lv_obj_t *s_bright_label = nullptr;
int s_set_hour = 0;
int s_set_minute = 0;
bool s_time_changed = false;
UiSettings s_set_values = {false, true, BRUSH_LANG_EN, CLOCK_FACE_CLASSIC, 100, true, 120};
constexpr uint16_t kDurations[] = {120, 100, 60, 40}; // selectable brushing times (seconds)
lv_obj_t *s_set_title = nullptr;
lv_obj_t *s_lang_label = nullptr;
lv_obj_t *s_face_label = nullptr;
lv_obj_t *s_font_label = nullptr;
lv_obj_t *s_duration_label = nullptr;
lv_obj_t *s_ok_label = nullptr;

lv_opa_t to_opa(float o)
{
    if (o <= 0.0f) {
        return LV_OPA_TRANSP;
    }
    if (o >= 1.0f) {
        return LV_OPA_COVER;
    }
    return (lv_opa_t)lroundf(o * 255.0f);
}

lv_opa_t scale_opa(lv_opa_t a, lv_opa_t b) { return (lv_opa_t)((uint16_t)a * b / 255); }

void set_visible(lv_obj_t *obj, bool visible)
{
    if (visible == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        if (visible) {
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Setting a style property always invalidates the object, even if the value is unchanged. These are
// called on every UI update, and the seconds ring covers the whole screen, so only set real changes.
void set_text_color(lv_obj_t *obj, lv_color_t color)
{
    if (lv_color_to16(lv_obj_get_style_text_color(obj, LV_PART_MAIN)) != lv_color_to16(color)) {
        lv_obj_set_style_text_color(obj, color, 0);
    }
}

void set_text_opa(lv_obj_t *obj, lv_opa_t opa)
{
    if (lv_obj_get_style_text_opa(obj, LV_PART_MAIN) != opa) {
        lv_obj_set_style_text_opa(obj, opa, 0);
    }
}

void set_arc_style(lv_obj_t *obj, lv_part_t part, lv_color_t color, lv_opa_t opa)
{
    if (lv_color_to16(lv_obj_get_style_arc_color(obj, part)) != lv_color_to16(color)) {
        lv_obj_set_style_arc_color(obj, color, part);
    }
    if (lv_obj_get_style_arc_opa(obj, part) != opa) {
        lv_obj_set_style_arc_opa(obj, opa, part);
    }
}

int index_of_color(int color) { return brush_index_of_color(s_core, (uint8_t)color); }

lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(l, text);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

lv_obj_t *make_arc(lv_obj_t *parent)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_remove_style_all(arc);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_range(arc, 0, 1000);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    return arc;
}

// ---- Event handlers ------------------------------------------------------------------

// Taps and swipes on the idle clock are told apart on release by how far the finger moved, so a
// slow swipe never falls through as a tap (which would start a timer).
constexpr lv_coord_t TAP_SLOP = 24;  // max movement for a tap or long press
constexpr lv_coord_t SWIPE_MIN = 40; // min horizontal travel for a swipe
lv_point_t s_press_start = {0, 0};
bool s_press_moved = false, s_press_long = false;

void idle_event_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    const lv_coord_t dx = p.x - s_press_start.x, dy = p.y - s_press_start.y;
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        s_press_start = p;
        s_press_moved = s_press_long = false;
        return;
    case LV_EVENT_PRESSING:
        if (LV_ABS(dx) > TAP_SLOP || LV_ABS(dy) > TAP_SLOP) {
            s_press_moved = true;
        }
        return;
    case LV_EVENT_LONG_PRESSED:
        if (!s_press_moved && s_core->count == 0) {
            s_press_long = true;
            app_open_settings();
        }
        return;
    case LV_EVENT_RELEASED:
        break;
    default:
        return;
    }
    if (s_press_long || s_core->count != 0) {
        return;
    }
    if (LV_ABS(dx) >= SWIPE_MIN && LV_ABS(dx) > LV_ABS(dy)) {
        app_change_clock_face(dx < 0);
    } else if (!s_press_moved) {
        app_start_timer();
    }
}

void card_event_cb(lv_event_t *e)
{
    int index = index_of_color((int)(intptr_t)lv_event_get_user_data(e));
    if (index < 0) {
        return; // ring is fading out
    }
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        app_timer_tapped(index);
    } else if (code == LV_EVENT_LONG_PRESSED) {
        app_timer_long_pressed(index);
    }
}

void add_event_cb(lv_event_t *) { app_start_timer(); }

void gear_event_cb(lv_event_t *) { app_open_settings(); }

// ---- Idle clock ------------------------------------------------------------------------

lv_color_t synth_palette_color(const uint32_t (&colors)[4], float progress)
{
    const float phase = progress * 4.0f;
    const int index = (int)phase;
    return lv_color_mix(lv_color_hex(colors[(index + 1) % 4]), lv_color_hex(colors[index]),
                        to_opa(phase - index));
}

void draw_synth_ring(lv_event_t *e)
{
    if (s_synth_opa <= LV_OPA_MIN && s_candy_opa <= LV_OPA_MIN) {
        return;
    }
    lv_obj_draw_part_dsc_t *part = lv_event_get_draw_part_dsc(e);
    if (part->type != LV_ARC_DRAW_PART_FOREGROUND || part->part != LV_PART_INDICATOR) {
        return;
    }
    lv_draw_arc_dsc_t arc = *part->arc_dsc;
    part->arc_dsc->opa = LV_OPA_TRANSP;
    if (arc.opa <= LV_OPA_MIN) {
        return;
    }
    const int first = lv_arc_get_angle_start(s_seconds_arc);
    const int sweep = lv_arc_get_angle_end(s_seconds_arc);
    const bool candy = s_candy_opa > LV_OPA_MIN;
    const int step = candy ? 6 : 5;
    arc.rounded = false;
    const lv_area_t &clip = *part->draw_ctx->clip_area;
    for (int start = first - first % step; start < sweep; start += step) {
        const int from = LV_MAX(start, first);
        const int end = LV_MIN(start + step, sweep);
        lv_area_t bounds;
        lv_draw_arc_get_area(part->p1->x, part->p1->y, part->radius, 270 + from, 270 + end,
                             arc.width, true, &bounds);
        if (bounds.x2 < clip.x1 || bounds.x1 > clip.x2 || bounds.y2 < clip.y1 || bounds.y1 > clip.y2) {
            continue;
        }
        // Sample the fixed palette, not the length of the visible sweep.
        arc.color = candy ? lv_color_hex((start / step) % 2 ? CANDY_WHITE : CANDY_RED)
                          : synth_palette_color(kSynthRingColors, (start + step * 0.5f) / 360.0f);
        lv_draw_arc(part->draw_ctx, &arc, part->p1, part->radius, 270 + from, 270 + end);
    }
}

// Mirrors synthBackdrop() in preview/sim.js.
void draw_synth_backdrop(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    const lv_area_t &box = s_synth_backdrop->coords;
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_opa = s_synth_opa;
    const auto fill = [&](int x, int y, int w, int h, lv_color_t color, lv_coord_t radius) {
        rect.bg_color = color;
        rect.radius = radius;
        const lv_area_t area = {(lv_coord_t)(box.x1 + x), (lv_coord_t)(box.y1 + y),
                                (lv_coord_t)(box.x1 + x + w - 1), (lv_coord_t)(box.y1 + y + h - 1)};
        lv_draw_rect(ctx, &rect, &area);
    };
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.opa = s_synth_opa;
    const auto line = [&](int x, int y, int end_x, int end_y, lv_color_t color, lv_coord_t width) {
        dsc.color = color;
        dsc.width = width;
        const lv_point_t a = {(lv_coord_t)(box.x1 + x), (lv_coord_t)(box.y1 + y)};
        const lv_point_t b = {(lv_coord_t)(box.x1 + end_x), (lv_coord_t)(box.y1 + end_y)};
        lv_draw_line(ctx, &dsc, &a, &b);
    };

    constexpr int stars[][3] = {{137, 65, 2}, {172, 115, 3}, {307, 56, 2}, {343, 92, 3},
                                {118, 130, 2}, {383, 145, 2}, {94, 210, 2}, {387, 244, 2}};
    for (const auto &star : stars) {
        fill(star[0], star[1], star[2], star[2], lv_color_hex(0x5D467E), LV_RADIUS_CIRCLE);
    }

    constexpr int sun_y = 97, sun_r = 51;
    for (int dy = -50; dy <= 50; dy += 2) {
        if ((dy >= 14 && dy < 18) || (dy >= 26 && dy < 32) || (dy >= 38 && dy < 46)) {
            continue;
        }
        const int half = (int)sqrtf((float)(sun_r * sun_r - dy * dy));
        fill(240 - half, sun_y + dy, 2 * half + 1, 2,
             lv_color_mix(lv_color_hex(0xFFB660), lv_color_hex(0xF02D91), to_opa((50 - dy) / 100.0f)), 0);
    }

    constexpr int horizon = SYNTH_HORIZON, clip_r = 228;
    const lv_color_t horizon_color = lv_color_mix(lv_color_hex(0xEFD4FF), s_synth_grid, LV_OPA_30);
    for (int ray = -8; ray <= 8; ++ray) {
        const int x = ray * 11, y = horizon - 240, dx = ray * 43, dy = 148;
        // Intersect the ray with the inset circle.
        const float a = dx * dx + dy * dy, b = x * dx + y * dy;
        const float c = x * x + y * y - clip_r * clip_r;
        const float end = (sqrtf(b * b - a * c) - b) / a;
        line(240 + x, horizon, 240 + x + lroundf(dx * end), horizon + lroundf(dy * end), s_synth_grid, 1);
    }
    constexpr int rows[] = {horizon, 371, 379, 391, 408, 431, 460};
    for (const int row : rows) {
        const int dy = row - 240;
        const int half = (int)sqrtf((float)(clip_r * clip_r - dy * dy));
        line(240 - half, row, 240 + half, row, row == horizon ? horizon_color : s_synth_grid,
             row == horizon ? 2 : 1);
    }
}

// Mirrors flowerBackdrop() in preview/sim.js. Each elapsed second lights one mark.
void draw_flower_backdrop(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    const lv_area_t &box = s_flower_backdrop->coords;
    const lv_area_t &clip = *ctx->clip_area;
    const lv_point_t center = {(lv_coord_t)(box.x1 + 240), (lv_coord_t)(box.y1 + 240)};
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.radius = LV_RADIUS_CIRCLE;
    rect.bg_opa = s_flower_opa;
    const auto circle = [&](int x, int y, int r, uint32_t color) {
        x += box.x1;
        y += box.y1;
        if (x + r < clip.x1 || x - r > clip.x2 || y + r < clip.y1 || y - r > clip.y2) {
            return;
        }
        rect.bg_color = lv_color_hex(color);
        const lv_area_t area = {(lv_coord_t)(x - r), (lv_coord_t)(y - r),
                                (lv_coord_t)(x + r), (lv_coord_t)(y + r)};
        lv_draw_rect(ctx, &rect, &area);
    };
    const auto daisy = [&](int x, int y, int reach, int petal, uint32_t color, uint32_t middle) {
        const int diag = (reach * 71 + 50) / 100;
        const int offsets[][2] = {{0, -reach}, {diag, -diag}, {reach, 0}, {diag, diag},
                                  {0, reach}, {-diag, diag}, {-reach, 0}, {-diag, -diag}};
        for (const auto &p : offsets) {
            circle(x + p[0], y + p[1], petal, color);
        }
        circle(x, y, reach / 2 + 1, middle);
    };

    circle(240, 240, 238, FLOWER_INK);
    circle(240, 240, 211, FLOWER_GOLD);
    circle(240, 240, 207, FLOWER_PAPER);

    if (clip.y2 >= box.y1 + 358) {
        lv_draw_arc_dsc_t ribbon;
        lv_draw_arc_dsc_init(&ribbon);
        ribbon.opa = s_flower_opa;
        ribbon.width = 11;
        ribbon.rounded = true;
        ribbon.color = lv_color_hex(FLOWER_PINK);
        lv_draw_arc(ctx, &ribbon, &center, 197, 50, 130);
        ribbon.color = lv_color_hex(FLOWER_CORAL);
        lv_draw_arc(ctx, &ribbon, &center, 183, 50, 130);
        ribbon.color = lv_color_hex(FLOWER_GOLD);
        lv_draw_arc(ctx, &ribbon, &center, 169, 50, 130);
    }

    daisy(236, 99, 24, 18, FLOWER_CORAL, FLOWER_INK);
    circle(236, 99, 8, FLOWER_GOLD);
    daisy(278, 124, 11, 8, FLOWER_PINK, FLOWER_GOLD);
    daisy(193, 124, 8, 6, FLOWER_GOLD, FLOWER_INK);
    daisy(115, 358, 14, 10, FLOWER_PINK, FLOWER_GOLD);
    daisy(143, 379, 9, 6, FLOWER_CORAL, FLOWER_INK);
    daisy(364, 358, 14, 10, FLOWER_GOLD, FLOWER_INK);
    daisy(341, 379, 9, 6, FLOWER_GREEN, FLOWER_GOLD);
    circle(185, 98, 3, FLOWER_GREEN);
    circle(282, 89, 4, FLOWER_GREEN);
    circle(297, 111, 2, FLOWER_CORAL);
    circle(91, 342, 3, FLOWER_CORAL);
    circle(389, 342, 3, FLOWER_PINK);

    if (s_flower_ring_opa <= LV_OPA_MIN) {
        return;
    }
    rect.bg_opa = s_flower_ring_opa;
    for (int i = 0; i < FLOWER_MARKS; ++i) {
        const lv_point_t &p = s_flower_marks[i];
        const bool current = i == s_flower_second;
        const bool past = i < s_flower_second;
        const uint32_t color = current || past ? kFlowerRingColors[i / 15] : 0x916478;
        if (i % 5 == 0) {
            daisy(p.x, p.y, current ? 5 : 4, current ? 3 : 2, color,
                  current ? FLOWER_PAPER : FLOWER_INK);
        } else {
            circle(p.x, p.y, current ? 4 : past ? 2 : 1, color);
            if (current) {
                circle(p.x, p.y, 1, FLOWER_PAPER);
            }
        }
    }
}

void update_flower_face(lv_opa_t opa, lv_opa_t ring_opa, int second)
{
    const bool full = opa != s_flower_opa || ring_opa != s_flower_ring_opa;
    const int previous = s_flower_second;
    s_flower_opa = opa;
    s_flower_ring_opa = ring_opa;
    s_flower_second = second;
    set_visible(s_flower_backdrop, opa > LV_OPA_MIN);
    if (opa <= LV_OPA_MIN) {
        return;
    }
    if (full || (ring_opa > LV_OPA_MIN && (second < previous || second - previous > 5))) {
        lv_obj_invalidate(s_flower_backdrop);
    } else if (ring_opa > LV_OPA_MIN && second != previous) {
        const lv_area_t &box = s_flower_backdrop->coords;
        for (int i = previous; i <= second; ++i) {
            const int x = box.x1 + s_flower_marks[i].x, y = box.y1 + s_flower_marks[i].y;
            const lv_area_t area = {(lv_coord_t)(x - 9), (lv_coord_t)(y - 9),
                                    (lv_coord_t)(x + 9), (lv_coord_t)(y + 9)};
            lv_obj_invalidate_area(s_flower_backdrop, &area);
        }
    }
}

void draw_christmas_backdrop(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    const lv_area_t &box = s_christmas_backdrop->coords;
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_opa = s_christmas_opa;
    rect.radius = LV_RADIUS_CIRCLE;
    const auto circle = [&](int x, int y, int r, uint32_t color) {
        rect.bg_color = lv_color_hex(color);
        const lv_area_t area = {(lv_coord_t)(box.x1 + x - r), (lv_coord_t)(box.y1 + y - r),
                                (lv_coord_t)(box.x1 + x + r), (lv_coord_t)(box.y1 + y + r)};
        lv_draw_rect(ctx, &rect, &area);
    };
    lv_draw_line_dsc_t stroke;
    lv_draw_line_dsc_init(&stroke);
    stroke.opa = s_christmas_opa;
    const auto line = [&](int x, int y, int end_x, int end_y, int width, uint32_t color) {
        stroke.width = width;
        stroke.color = lv_color_hex(color);
        const lv_point_t a = {(lv_coord_t)(box.x1 + x), (lv_coord_t)(box.y1 + y)};
        const lv_point_t b = {(lv_coord_t)(box.x1 + end_x), (lv_coord_t)(box.y1 + end_y)};
        lv_draw_line(ctx, &stroke, &a, &b);
    };

    circle(240, 240, 221, 0x36554A);
    circle(240, 240, 219, 0x112A25);
    constexpr int flurries[][3] = {{137, 64, 2}, {166, 115, 2}, {125, 141, 2}, {87, 181, 2},
        {69, 251, 2}, {129, 342, 2}, {107, 360, 1}, {156, 385, 2}, {323, 63, 2},
        {312, 109, 2}, {359, 135, 2}, {388, 181, 2}, {414, 242, 2}, {360, 348, 2},
        {377, 360, 1}, {323, 376, 2}};
    for (const auto &p : flurries) {
        circle(p[0], p[1], p[2], 0x789F90);
    }

    constexpr uint32_t thread = 0xA08354;
    line(203, 54, 203, 101, 1, thread);
    line(277, 54, 277, 101, 1, thread);
    line(240, 41, 240, 68, 1, thread);
    circle(203, 111, 12, 0xAD3C46);
    circle(200, 107, 3, 0xE08C85);
    circle(277, 111, 12, 0xAD3C46);
    circle(274, 107, 3, 0xE08C85);
    line(197, 99, 209, 99, 3, CHRISTMAS_GOLD);
    line(271, 99, 283, 99, 3, CHRISTMAS_GOLD);
    line(226, 94, 254, 94, 2, CHRISTMAS_GOLD);
    line(240, 68, 240, 120, 3, CHRISTMAS_GOLD);
    line(227, 81, 253, 107, 2, CHRISTMAS_GOLD);
    line(227, 107, 253, 81, 2, CHRISTMAS_GOLD);
    circle(240, 94, 6, CHRISTMAS_SNOW);

    const auto pine = [&](int x, int top, int span, int step, uint32_t color) {
        line(x, top, x, top + 5 * step, 2, color);
        for (int i = 0; i < 3; ++i) {
            const int y = top + i * step;
            const int w = span * (i + 2) / 4;
            line(x, y, x - w, y + 2 * step, 3, color);
            line(x, y, x + w, y + 2 * step, 3, color);
        }
    };
    pine(169, 385, 15, 7, 0x648C7A);
    pine(204, 377, 18, 9, 0x9CC2AF);
    pine(240, 369, 26, 13, 0xC5E0C8);
    pine(276, 377, 18, 9, 0x9CC2AF);
    pine(311, 385, 15, 7, 0x648C7A);
    circle(233, 399, 2, CHRISTMAS_GOLD);
    circle(245, 396, 2, CHRISTMAS_GOLD);
    circle(249, 412, 2, CHRISTMAS_GOLD);
    circle(228, 407, 2, CHRISTMAS_GOLD);

    const lv_point_t center = {(lv_coord_t)(box.x1 + 240), (lv_coord_t)(box.y1 + 240)};
    lv_draw_arc_dsc_t ground;
    lv_draw_arc_dsc_init(&ground);
    ground.opa = s_christmas_opa;
    ground.width = 2;
    ground.rounded = true;
    ground.color = lv_color_hex(0x648C7A);
    lv_draw_arc(ctx, &ground, &center, 197, 63, 117);
    ground.color = lv_color_hex(0x36554A);
    lv_draw_arc(ctx, &ground, &center, 204, 71, 109);
}

lv_point_t pt(float x, float y) { return {(lv_coord_t)lroundf(x), (lv_coord_t)lroundf(y)}; }

// Danish woven Christmas heart in felt, with a bow and embroidery. Mirrors heartFace() in preview/sim.js.
template <typename Circle, typename Line, typename Poly, typename Arc, typename Flake>
void draw_heart_face(const Circle &circle, const Line &line, const Poly &poly, const Arc &arc, const Flake &snowflake)
{
    constexpr uint32_t felt = 0x163A2A, felt_rim = 0x0E2A1D, green = 0x3E8A55, cream = 0xF1E6CC;
    constexpr uint32_t stitch_green = 0x2B6640, red = 0xD6343B, dark_red = 0x9E2229, muted = 0x7FA58B;
    circle(240, 240, 221, felt_rim);
    circle(240, 240, 219, felt);
    for (int a = 0; a < 360; a += 6) {
        arc(240, 240, 212, 2, a, a + 3, cream, false); // running stitch around the dial
    }

    // Hanging loop, partly covered by the heart and the bow.
    poly({pt(234, 28), pt(246, 28), pt(246, 70), pt(234, 70)}, green);
    circle(240, 34, 6, green);
    for (int y = 32; y < 62; y += 7) {
        line(237, y, 237, y + 4, 1, cream);
        line(243, y, 243, y + 4, 1, cream);
    }

    // Heart: a diamond of woven strips with a half-disc lobe on each upper edge.
    constexpr float hx = 240.0f, hy = 106.0f, d = 38.0f, lobe = d * 0.70710678f;
    circle(lroundf(hx - d / 2), lroundf(hy - d / 2), lroundf(lobe), green);
    circle(lroundf(hx + d / 2), lroundf(hy - d / 2), lroundf(lobe), cream);
    // (a, b) run along the left->top and left->bottom edges.
    const auto at = [&](float a, float b) { return pt(hx - d + (a + b) * d, hy + (b - a) * d); };
    constexpr int n = 4;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            const float a0 = (float)i / n, a1 = (float)(i + 1) / n, b0 = (float)j / n, b1 = (float)(j + 1) / n;
            const bool light = (i + j) % 2 != 0;
            poly({at(a0, b0), at(a1, b0), at(a1, b1), at(a0, b1)}, light ? cream : green);
            if (light) {
                const lv_point_t c = at((a0 + a1) / 2, (b0 + b1) / 2);
                line(c.x - 3, c.y, c.x + 3, c.y, 1, stitch_green);
                line(c.x, c.y - 3, c.x, c.y + 3, 1, stitch_green);
            }
        }
    }
    // Blanket-stitch outline: around both lobes and down the two lower edges.
    const int lobe_r = (int)lroundf(lobe) + 5;
    for (int a = 135; a < 315; a += 15) {
        arc(lroundf(hx - d / 2), lroundf(hy - d / 2), lobe_r, 2, a, a + 7, cream, false);
        arc(lroundf(hx + d / 2), lroundf(hy - d / 2), lobe_r, 2, a + 90, a + 97, cream, false);
    }
    for (int k = 0; k < 6; k++) {
        const float t0 = (k + 0.15f) / 6.0f, t1 = (k + 0.6f) / 6.0f;
        constexpr float off = 3.5f; // outward along the edge normal
        line(lroundf(hx - d + t0 * d - off), lroundf(hy + t0 * d + off),
             lroundf(hx - d + t1 * d - off), lroundf(hy + t1 * d + off), 2, cream);
        line(lroundf(hx + d - t0 * d + off), lroundf(hy + t0 * d + off),
             lroundf(hx + d - t1 * d + off), lroundf(hy + t1 * d + off), 2, cream);
    }

    // Embroidery: a snowflake on the green lobe, a berry sprig on the cream one.
    snowflake(lroundf(hx - d / 2 - 3), lroundf(hy - d / 2 - 3), 8, cream);
    line(259, 104, 266, 78, 1, stitch_green);
    line(263, 92, 256, 86, 1, stitch_green);
    constexpr int berries[][2] = {{266, 76}, {270, 84}, {255, 84}, {262, 98}, {268, 94}};
    for (const auto &b : berries) {
        circle(b[0], b[1], 2, red);
    }

    // Bow with two felt pom-poms where the loop meets the heart.
    poly({pt(237, 70), pt(226, 90), pt(233, 92), pt(240, 74)}, red);
    poly({pt(243, 70), pt(254, 90), pt(247, 92), pt(240, 74)}, red);
    circle(229, 64, 9, red);
    circle(251, 64, 9, red);
    circle(229, 64, 3, dark_red);
    circle(251, 64, 3, dark_red);
    circle(240, 68, 5, dark_red);
    circle(236, 74, 4, 0xFAF6EE);
    circle(244, 74, 4, 0xFAF6EE);

    // Holly sprig at the bottom.
    const auto leaf = [&](float bx, float by, float tx, float ty) {
        const float vx = tx - bx, vy = ty - by, len = sqrtf(vx * vx + vy * vy);
        const float nx = -vy / len * 7.0f, ny = vx / len * 7.0f;
        poly({pt(bx, by), pt(bx + vx / 3 + nx, by + vy / 3 + ny), pt(bx + vx * 2 / 3 + nx, by + vy * 2 / 3 + ny),
              pt(tx, ty), pt(bx + vx * 2 / 3 - nx, by + vy * 2 / 3 - ny), pt(bx + vx / 3 - nx, by + vy / 3 - ny)},
             green);
        line(lroundf(bx), lroundf(by), lroundf(tx), lroundf(ty), 1, stitch_green);
    };
    leaf(240, 398, 203, 386);
    leaf(240, 398, 277, 386);
    leaf(240, 398, 240, 372);
    circle(234, 402, 5, red);
    circle(246, 402, 5, red);
    circle(240, 394, 5, red);
    circle(232, 400, 1, 0xF6B0B0);
    circle(244, 400, 1, 0xF6B0B0);

    snowflake(110, 128, 7, muted);
    snowflake(370, 128, 7, muted);
    snowflake(98, 342, 6, muted);
    snowflake(382, 342, 6, muted);
}

// Candy canes bent into a heart, peppermints and sprinkles. Mirrors candyFace() in preview/sim.js.
template <typename Circle, typename Line, typename Poly, typename Arc>
void draw_candy_face(const Circle &circle, const Line &line, const Poly &poly, const Arc &arc)
{
    constexpr uint32_t cream = 0xFFF3E8, rim = 0xC8102E, outline = 0xE7B9C1;
    circle(240, 240, 221, rim);
    circle(240, 240, 217, cream);

    constexpr int sprinkles[][5] = {{126, 118, 7, -4, 0xF48FB1}, {150, 92, 7, 3, 0x7FD8BE}, {336, 94, -7, 3, 0xFFD166},
        {360, 120, 6, 5, 0x7EC8E3}, {62, 205, 3, 7, 0xFFD166}, {418, 292, -3, 7, 0xF48FB1},
        {112, 334, 7, 2, 0x7EC8E3}, {368, 334, -7, 2, 0x7FD8BE}, {80, 280, 6, -4, 0x7FD8BE},
        {404, 172, -6, -4, 0xF48FB1}, {196, 52, 7, 2, 0xFFD166}, {286, 52, -7, 2, 0x7EC8E3}};
    for (const auto &s : sprinkles) {
        line(s[0] - s[2] / 2, s[1] - s[3] / 2, s[0] + s[2] / 2, s[1] + s[3] / 2, 4, (uint32_t)s[4]);
    }

    // Two candy canes whose hooks meet at the top: a heart.
    constexpr int r = 18, w = 9, cy = 84, tip_y = 148;
    for (int side = -1; side <= 1; side += 2) {
        const int cx = 240 + side * r, outer_x = 240 + side * 2 * r;
        // Outline first so white stripes stand out from the cream dial.
        arc(cx, cy, r + w / 2 + 2, w + 4, 180, 360, outline, false);
        line(outer_x, cy, 240, tip_y, w + 4, outline);
        int stripe = 0;
        for (int a = 180; a < 360; a += 30, stripe++) {
            arc(cx, cy, r + w / 2, w, a, a + 30, stripe % 2 ? CANDY_WHITE : CANDY_RED, false);
        }
        constexpr int pieces = 8;
        for (int k = 0; k < pieces; k++, stripe++) {
            line(outer_x + (240 - outer_x) * k / pieces, cy + (tip_y - cy) * k / pieces,
                 outer_x + (240 - outer_x) * (k + 1) / pieces, cy + (tip_y - cy) * (k + 1) / pieces, w,
                 stripe % 2 ? CANDY_WHITE : CANDY_RED);
        }
    }

    // Peppermint swirls.
    const auto peppermint = [&](int x, int y, int pr) {
        circle(x, y, pr + 2, CANDY_RED);
        circle(x, y, pr, CANDY_WHITE);
        for (int a = 0; a < 360; a += 45) {
            arc(x, y, pr - 2, pr - 4, a, a + 22, CANDY_RED, false);
        }
        circle(x, y, LV_MAX(3, pr / 5), CANDY_WHITE);
    };
    peppermint(176, 392, 16);
    peppermint(304, 392, 16);
    peppermint(240, 404, 22);
    poly({pt(214, 404), pt(204, 396), pt(204, 412)}, 0xF7C5CF); // wrapper twists
    poly({pt(266, 404), pt(276, 396), pt(276, 412)}, 0xF7C5CF);
}

void draw_extra_backdrop(lv_event_t *e)
{
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    const lv_area_t &box = s_extra_backdrop->coords;
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_opa = s_extra_opa;
    const auto circle = [&](int x, int y, int r, uint32_t color) {
        rect.radius = LV_RADIUS_CIRCLE;
        rect.bg_color = lv_color_hex(color);
        const lv_area_t area = {(lv_coord_t)(box.x1 + x - r), (lv_coord_t)(box.y1 + y - r),
                                (lv_coord_t)(box.x1 + x + r), (lv_coord_t)(box.y1 + y + r)};
        lv_draw_rect(ctx, &rect, &area);
    };
    lv_draw_line_dsc_t stroke;
    lv_draw_line_dsc_init(&stroke);
    stroke.opa = s_extra_opa;
    const auto line = [&](int x, int y, int end_x, int end_y, int width, uint32_t color) {
        stroke.width = width;
        stroke.color = lv_color_hex(color);
        const lv_point_t a = {(lv_coord_t)(box.x1 + x), (lv_coord_t)(box.y1 + y)};
        const lv_point_t b = {(lv_coord_t)(box.x1 + end_x), (lv_coord_t)(box.y1 + end_y)};
        lv_draw_line(ctx, &stroke, &a, &b);
    };
    lv_draw_rect_dsc_t fill;
    lv_draw_rect_dsc_init(&fill);
    fill.bg_opa = s_extra_opa;
    // Convex polygons only (LVGL limitation).
    const auto poly = [&](std::initializer_list<lv_point_t> points, uint32_t color) {
        lv_point_t abs_points[8];
        uint16_t n = 0;
        for (const lv_point_t &p : points) {
            abs_points[n++] = {(lv_coord_t)(box.x1 + p.x), (lv_coord_t)(box.y1 + p.y)};
        }
        fill.bg_color = lv_color_hex(color);
        lv_draw_polygon(ctx, &fill, abs_points, n);
    };
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.opa = s_extra_opa;
    // `r` is the outer radius, like lv_draw_arc().
    const auto arc = [&](int x, int y, int r, int width, int start, int end, uint32_t color, bool rounded) {
        arc_dsc.color = lv_color_hex(color);
        arc_dsc.width = (lv_coord_t)width;
        arc_dsc.rounded = rounded;
        const lv_point_t c = {(lv_coord_t)(box.x1 + x), (lv_coord_t)(box.y1 + y)};
        lv_draw_arc(ctx, &arc_dsc, &c, (uint16_t)r, (uint16_t)start, (uint16_t)end);
    };
    const auto snowflake = [&](int x, int y, int r, uint32_t color) {
        const int d = r * 7 / 8, h = r / 2;
        line(x, y - r, x, y + r, 1, color);
        line(x - d, y - h, x + d, y + h, 1, color);
        line(x - d, y + h, x + d, y - h, 1, color);
    };

    if (s_extra_face == CLOCK_FACE_HEART) {
        draw_heart_face(circle, line, poly, arc, snowflake);
        return;
    }
    if (s_extra_face == CLOCK_FACE_CANDY) {
        draw_candy_face(circle, line, poly, arc);
        return;
    }
    if (s_extra_face == CLOCK_FACE_NINETIES) {
        circle(240, 240, 221, 0x7A59C9);
        circle(240, 240, 218, 0x211638);
        constexpr int dots[][4] = {{150, 100, 5, 0xFF69B4}, {330, 100, 5, 0xFFE56D},
            {75, 190, 4, 0x63EBD0}, {405, 205, 5, 0xFF69B4}, {105, 290, 5, 0xFFE56D},
            {375, 295, 4, 0x63EBD0}, {160, 382, 5, 0xFF69B4}, {320, 382, 5, 0xFFE56D}};
        for (const auto &dot : dots) circle(dot[0], dot[1], dot[2], (uint32_t)dot[3]);
        line(192, 88, 206, 75, 4, 0x63EBD0);
        line(206, 75, 220, 88, 4, 0x63EBD0);
        line(220, 88, 234, 75, 4, 0x63EBD0);
        line(252, 81, 266, 96, 4, 0xFF69B4);
        line(266, 96, 280, 81, 4, 0xFF69B4);
        line(280, 81, 294, 96, 4, 0xFF69B4);
        line(189, 391, 210, 375, 4, 0xFFE56D);
        line(210, 375, 228, 391, 4, 0xFFE56D);
        line(252, 391, 270, 375, 4, 0x63EBD0);
        line(270, 375, 291, 391, 4, 0x63EBD0);
        line(120, 139, 131, 150, 3, 0xFFE56D);
        line(131, 139, 120, 150, 3, 0xFFE56D);
        line(349, 139, 360, 150, 3, 0x63EBD0);
        line(360, 139, 349, 150, 3, 0x63EBD0);
    } else {
        circle(240, 240, 221, 0x527897);
        circle(240, 240, 218, 0x102B43);
        constexpr int snow[][3] = {{150, 92, 3}, {188, 65, 2}, {240, 57, 3}, {293, 67, 2},
            {330, 94, 3}, {116, 145, 2}, {363, 145, 2}, {79, 205, 3}, {400, 205, 3},
            {75, 274, 2}, {404, 274, 2}, {112, 325, 3}, {368, 325, 3},
            {158, 367, 2}, {322, 367, 2}, {183, 401, 3}, {297, 401, 3}};
        for (const auto &flake : snow) circle(flake[0], flake[1], flake[2], 0xB9DAE8);
        constexpr uint32_t antler = 0xD8B08A;
        line(224, 401, 209, 380, 4, antler);
        line(209, 380, 208, 367, 4, antler);
        line(211, 384, 198, 379, 4, antler);
        line(256, 401, 271, 380, 4, antler);
        line(271, 380, 272, 367, 4, antler);
        line(269, 384, 282, 379, 4, antler);
        circle(216, 406, 9, 0xB7794E);
        circle(264, 406, 9, 0xB7794E);
        circle(240, 412, 25, 0xB7794E);
        circle(232, 408, 2, 0x172638);
        circle(248, 408, 2, 0x172638);
        circle(240, 421, 10, 0xE2B993);
        circle(240, 419, 5, 0xE74F5F);
    }
}

bool is_birthday(const struct tm &date)
{
    for (const Birthday &birthday : kBirthdays) {
        if (birthday.month == date.tm_mon + 1 && birthday.day == date.tm_mday) {
            return true;
        }
    }
    return false;
}

void draw_birthday_flag(lv_event_t *e)
{
    const auto &flag = *static_cast<const BirthdayFlag *>(lv_event_get_user_data(e));
    const lv_area_t &box = flag.obj->coords;
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_opa = flag.opa;
    const auto fill = [&](lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_color_t color) {
        const lv_area_t area = {(lv_coord_t)(box.x1 + x), (lv_coord_t)(box.y1 + y),
                               (lv_coord_t)(box.x1 + x + w - 1), (lv_coord_t)(box.y1 + y + h - 1)};
        rect.bg_color = color;
        lv_draw_rect(ctx, &rect, &area);
    };

    fill(2, 4, 2, 76, lv_color_hex(0x88818A));
    fill(1, 2, 4, 4, lv_color_hex(0xEAC77B));
    const float phase = 6.28318530718f * flag.phase_ms / FLAG_WAVE_MS;
    // Dannebrog proportions: 12:4:21 across and 12:4:12 down, with the cloth fixed at the pole.
    for (lv_coord_t x = 0; x < FLAG_CLOTH_WIDTH; x += 2) {
        const float u = (float)x / FLAG_CLOTH_WIDTH;
        const float fold = phase - 7.0f * u;
        const lv_coord_t y = 10 + (lv_coord_t)lroundf(8.0f * u * sinf(fold));
        const lv_opa_t light = to_opa(0.5f + (0.25f + 0.22f * u) * cosf(fold));
        const lv_color_t white = lv_color_mix(lv_color_hex(0xFFF9F8), lv_color_hex(0xE5DCE1), light);
        if (x >= 24 && x < 32) {
            fill(4 + x, y, 2, 56, white);
        } else {
            const lv_color_t red = lv_color_mix(lv_color_hex(0xDE2942), lv_color_hex(0xA81027), light);
            fill(4 + x, y, 2, 24, red);
            fill(4 + x, y + 24, 2, 8, white);
            fill(4 + x, y + 32, 2, 24, red);
        }
    }
}

void build_birthday_flags()
{
    for (auto &flag : s_flags) {
        flag.obj = lv_obj_create(s_view);
        lv_obj_remove_style_all(flag.obj);
        lv_obj_clear_flag(flag.obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(flag.obj, FLAG_BOX_WIDTH, 80);
        lv_obj_set_pos(flag.obj, flag.x, 70);
        set_visible(flag.obj, false);
        lv_obj_add_event_cb(flag.obj, draw_birthday_flag, LV_EVENT_DRAW_MAIN, &flag);
    }
}

void update_birthday_flags(uint32_t now_ms, const struct tm &date)
{
    const lv_opa_t opa = s_core->count == 0 && is_birthday(date)
                            ? scale_opa(s_idle_opa, to_opa(1.0f - s_saver_level))
                            : LV_OPA_TRANSP;
    for (auto &flag : s_flags) {
        set_visible(flag.obj, opa > LV_OPA_MIN);
        if (opa <= LV_OPA_MIN) {
            continue;
        }
        const uint32_t phase = ((now_ms % FLAG_WAVE_MS / FLAG_FRAME_MS) * FLAG_FRAME_MS +
                                flag.phase_offset_ms) % FLAG_WAVE_MS;
        if (phase != flag.phase_ms || opa != flag.opa) {
            flag.phase_ms = phase;
            flag.opa = opa;
            lv_obj_invalidate(flag.obj);
        }
    }
}

void draw_timer_bg(lv_event_t *e)
{
    const FacePalette &p = kFacePalettes[s_timer_bg_face];
    if (p.bg == 0 || s_timer_bg_opa <= LV_OPA_MIN) {
        return;
    }
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    const lv_area_t &box = s_timer_bg->coords;
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.radius = LV_RADIUS_CIRCLE;
    rect.bg_opa = s_timer_bg_opa;
    rect.bg_color = lv_color_hex(p.bg);
    rect.border_color = lv_color_hex(p.rim);
    rect.border_width = p.rim_style == RIM_CANDY ? 0 : 4;
    rect.border_opa = s_timer_bg_opa;
    // The candy stripes go around the outside of the dial so the white ones show against the bezel.
    const lv_coord_t inset = p.rim_style == RIM_CANDY ? 26 : 19;
    const lv_area_t dial = {(lv_coord_t)(box.x1 + inset), (lv_coord_t)(box.y1 + inset),
                            (lv_coord_t)(box.x1 + 480 - inset), (lv_coord_t)(box.y1 + 480 - inset)};
    lv_draw_rect(ctx, &rect, &dial);

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.opa = s_timer_bg_opa;
    const lv_point_t center = {(lv_coord_t)(box.x1 + 240), (lv_coord_t)(box.y1 + 240)};
    if (p.rim_style == RIM_CANDY) {
        arc.width = 6; // red/white candy-cane stripes around the dial
        for (int a = 0; a < 360; a += 8) {
            arc.color = lv_color_hex((a / 8) % 2 ? CANDY_WHITE : CANDY_RED);
            lv_draw_arc(ctx, &arc, &center, 221, (uint16_t)a, (uint16_t)(a + 8));
        }
    } else if (p.rim_style == RIM_STITCH) {
        arc.width = 2; // running stitch, as on the idle face
        arc.color = lv_color_hex(0xF1E6CC);
        for (int a = 0; a < 360; a += 6) {
            lv_draw_arc(ctx, &arc, &center, 212, (uint16_t)a, (uint16_t)(a + 3));
        }
    } else if (p.rim_style == RIM_FLOWER) {
        arc.width = 4; // gold ring inside the ink rim, as on the idle face
        arc.color = lv_color_hex(FLOWER_GOLD);
        lv_draw_arc(ctx, &arc, &center, 217, 0, 360);
    }
}

void build_idle()
{
    s_timer_bg = lv_obj_create(s_view);
    lv_obj_remove_style_all(s_timer_bg);
    lv_obj_clear_flag(s_timer_bg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_timer_bg, LCD_WIDTH, LCD_HEIGHT);
    set_visible(s_timer_bg, false);
    lv_obj_add_event_cb(s_timer_bg, draw_timer_bg, LV_EVENT_DRAW_MAIN, nullptr);

    s_synth_backdrop = lv_obj_create(s_view);
    lv_obj_remove_style_all(s_synth_backdrop);
    lv_obj_clear_flag(s_synth_backdrop, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_synth_backdrop, LCD_WIDTH, LCD_HEIGHT);
    set_visible(s_synth_backdrop, false);
    lv_obj_add_event_cb(s_synth_backdrop, draw_synth_backdrop, LV_EVENT_DRAW_MAIN, nullptr);

    s_flower_backdrop = lv_obj_create(s_view);
    lv_obj_remove_style_all(s_flower_backdrop);
    lv_obj_clear_flag(s_flower_backdrop, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_flower_backdrop, LCD_WIDTH, LCD_HEIGHT);
    set_visible(s_flower_backdrop, false);
    lv_obj_add_event_cb(s_flower_backdrop, draw_flower_backdrop, LV_EVENT_DRAW_MAIN, nullptr);
    for (int i = 0; i < FLOWER_MARKS; ++i) {
        const float angle = 6.28318530718f * i / FLOWER_MARKS;
        s_flower_marks[i] = {(lv_coord_t)(240 + lroundf(224 * sinf(angle))),
                             (lv_coord_t)(240 - lroundf(224 * cosf(angle)))};
    }

    s_christmas_backdrop = lv_obj_create(s_view);
    lv_obj_remove_style_all(s_christmas_backdrop);
    lv_obj_clear_flag(s_christmas_backdrop, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_christmas_backdrop, LCD_WIDTH, LCD_HEIGHT);
    set_visible(s_christmas_backdrop, false);
    lv_obj_add_event_cb(s_christmas_backdrop, draw_christmas_backdrop, LV_EVENT_DRAW_MAIN, nullptr);

    s_extra_backdrop = lv_obj_create(s_view);
    lv_obj_remove_style_all(s_extra_backdrop);
    lv_obj_clear_flag(s_extra_backdrop, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_extra_backdrop, LCD_WIDTH, LCD_HEIGHT);
    set_visible(s_extra_backdrop, false);
    lv_obj_add_event_cb(s_extra_backdrop, draw_extra_backdrop, LV_EVENT_DRAW_MAIN, nullptr);

    s_seconds_arc = lv_arc_create(s_view);
    lv_obj_remove_style_all(s_seconds_arc);
    lv_obj_clear_flag(s_seconds_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(s_seconds_arc, LCD_WIDTH - 12, LCD_HEIGHT - 12);
    lv_obj_center(s_seconds_arc);
    lv_arc_set_rotation(s_seconds_arc, 270);
    lv_arc_set_bg_angles(s_seconds_arc, 0, 360);
    lv_arc_set_range(s_seconds_arc, 0, 600);
    lv_obj_set_style_arc_width(s_seconds_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_seconds_arc, 4, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_seconds_arc, lv_color_hex(0x1A1A1C), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_seconds_arc, lv_color_hex(kTimerColors[0]), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(s_seconds_arc, true, LV_PART_INDICATOR);
    lv_obj_add_event_cb(s_seconds_arc, draw_synth_ring, LV_EVENT_DRAW_PART_BEGIN, nullptr);

    s_date = make_label(s_view, &tb_font_28, COLOR_MUTED, "");
    s_hint = make_label(s_view, &tb_font_20, COLOR_FAINT, s_txt->hint);
    lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, 372);

    s_gear = lv_obj_create(s_view);
    lv_obj_remove_style_all(s_gear);
    lv_obj_clear_flag(s_gear, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_gear, GEAR_SIZE, GEAR_SIZE);
    lv_obj_set_pos(s_gear, GEAR_CX - GEAR_SIZE / 2, GEAR_CY - GEAR_SIZE / 2);
    lv_obj_set_style_radius(s_gear, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_gear, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s_gear, 30, LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_gear, gear_event_cb, LV_EVENT_CLICKED, nullptr);
    s_gear_icon = make_label(s_gear, &tb_font_20, COLOR_FAINT, LV_SYMBOL_SETTINGS);
    lv_obj_center(s_gear_icon);

    s_clock = seg_create(s_view, 100);
    seg_set_text(s_clock, "00:00");
    // Idle-only clock in the face's own font; the segment clock takes over when timers start.
    s_font_clock = make_label(s_view, &tb_font_28, COLOR_TEXT, "00:00");
    set_visible(s_font_clock, false);
    build_birthday_flags();
}

// Top of a font-clock label so the digits (not the line box) are centered on the idle clock.
lv_coord_t font_clock_top(const lv_font_t *font)
{
    brush_layout_t l;
    brush_layout(0, &l);
    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(font, &g, '0', '\0')) {
        return (lv_coord_t)(l.clock_cy - font->line_height / 2);
    }
    const lv_coord_t baseline = (lv_coord_t)(font->line_height - font->base_line); // from the label top
    return (lv_coord_t)(l.clock_cy - (baseline - g.ofs_y - g.box_h / 2));
}

float update_screensaver_level(uint32_t now_ms, bool screensaver)
{
    if (screensaver != s_saver_target) {
        s_saver_target = screensaver;
        s_saver_from = s_saver_level;
        s_saver_start_ms = now_ms;
        if (screensaver) {
            s_saver_epoch_ms = now_ms;
        }
    }

    const uint32_t elapsed = now_ms - s_saver_start_ms;
    const float progress = LV_MIN((float)elapsed / SCREENSAVER_TRANSITION_MS, 1.0f);
    const float eased = progress * progress * (3.0f - 2.0f * progress);
    s_saver_level = s_saver_from + ((s_saver_target ? 1.0f : 0.0f) - s_saver_from) * eased;
    s_saver_transitioning = progress < 1.0f;
    return s_saver_level;
}

lv_color_t screensaver_color(uint32_t elapsed_ms)
{
    const uint32_t phase_ms = elapsed_ms % SCREENSAVER_COLOR_CYCLE_MS;
    const uint8_t color_index = (uint8_t)(phase_ms / SCREENSAVER_COLOR_SEGMENT_MS);
    float blend = (float)(phase_ms % SCREENSAVER_COLOR_SEGMENT_MS) / SCREENSAVER_COLOR_SEGMENT_MS;
    blend = blend * blend * (3.0f - 2.0f * blend);
    return lv_color_mix(lv_color_hex(kTimerColors[color_index]),
                        lv_color_hex(kTimerColors[(color_index + 1) % BRUSH_MAX_TIMERS]),
                        (lv_opa_t)(LV_OPA_COVER - to_opa(blend)));
}

void apply_idle(uint32_t now_ms, bool screensaver, ClockFace face, int second, bool custom_font)
{
    const float saver_level = update_screensaver_level(now_ms, screensaver);
    const lv_opa_t saver_opa = to_opa(saver_level);
    const uint32_t saver_elapsed = now_ms - s_saver_epoch_ms;
    const float ring_fade = LV_MIN(saver_level * 2.0f, 1.0f);
    lv_coord_t shift_x = 0, shift_y = 0;
    if (saver_elapsed > SCREENSAVER_DRIFT_DELAY_MS && saver_level > 0.5f) {
        const uint32_t drift_ms = saver_elapsed - SCREENSAVER_DRIFT_DELAY_MS;
        const float ramp = brush_ease(EASE_IN_OUT_CUBIC, (float)drift_ms / SCREENSAVER_TRANSITION_MS);
        const float radius = SCREENSAVER_SHIFT * ramp * (saver_level - 0.5f) * 2.0f;
        const float angle = 6.28318530718f * (float)(drift_ms % SCREENSAVER_POSITION_CYCLE_MS) /
                            SCREENSAVER_POSITION_CYCLE_MS;
        shift_x = (lv_coord_t)lroundf(radius * sinf(angle));
        shift_y = (lv_coord_t)lroundf(radius * cosf(angle));
    }
    const lv_color_t accent = screensaver_color(saver_elapsed);

    brush_geom_t g;
    brush_tween_value(&s_scene.idle, now_ms, &g);
    const lv_opa_t opa = to_opa(g.opa);
    const lv_opa_t theme_opa = scale_opa(opa, (lv_opa_t)(LV_OPA_COVER - saver_opa));
    const lv_opa_t ring_alpha = scale_opa(opa, (lv_opa_t)(LV_OPA_COVER - to_opa(ring_fade)));
    const bool flower = face == CLOCK_FACE_FLOWER_POWER;
    update_flower_face(flower ? theme_opa : LV_OPA_TRANSP, flower ? ring_alpha : LV_OPA_TRANSP, second);
    const bool christmas = face == CLOCK_FACE_CHRISTMAS;
    const bool extra = face >= CLOCK_FACE_NINETIES; // drawn by draw_extra_backdrop()
    const lv_opa_t extra_opa = extra ? theme_opa : LV_OPA_TRANSP;
    if (extra_opa != s_extra_opa || (extra && face != s_extra_face)) {
        s_extra_opa = extra_opa;
        if (extra) s_extra_face = face;
        set_visible(s_extra_backdrop, extra_opa > LV_OPA_MIN);
        lv_obj_invalidate(s_extra_backdrop);
    }
    s_candy_opa = face == CLOCK_FACE_CANDY ? theme_opa : LV_OPA_TRANSP;
    // While timers run, the decorated face fades into a plain dial in the face's colors.
    const lv_opa_t timer_bg = kFacePalettes[face].bg ? (lv_opa_t)(LV_OPA_COVER - opa) : LV_OPA_TRANSP;
    if (timer_bg != s_timer_bg_opa || face != s_timer_bg_face) {
        s_timer_bg_opa = timer_bg;
        s_timer_bg_face = face;
        set_visible(s_timer_bg, timer_bg > LV_OPA_MIN);
        lv_obj_invalidate(s_timer_bg);
    }
    const lv_opa_t christmas_opa = christmas ? theme_opa : LV_OPA_TRANSP;
    if (christmas_opa != s_christmas_opa) {
        s_christmas_opa = christmas_opa;
        set_visible(s_christmas_backdrop, christmas_opa > LV_OPA_MIN);
        lv_obj_invalidate(s_christmas_backdrop);
    }
    const lv_opa_t synth = face == CLOCK_FACE_SYNTHWAVE ? theme_opa : LV_OPA_TRANSP;
    const lv_color_t grid = synth > LV_OPA_MIN
        ? synth_palette_color(kSynthGridColors, (float)(now_ms % SYNTH_GRID_CYCLE_MS) / SYNTH_GRID_CYCLE_MS)
        : s_synth_grid;
    const bool grid_changed = lv_color_to16(grid) != lv_color_to16(s_synth_grid);
    s_synth_grid = grid;
    if (synth != s_synth_opa) {
        s_synth_opa = synth;
        set_visible(s_synth_backdrop, synth > LV_OPA_MIN);
        lv_obj_invalidate(s_synth_backdrop);
    } else if (grid_changed) {
        lv_area_t grid_area = s_synth_backdrop->coords;
        grid_area.y1 += SYNTH_HORIZON - 2;
        lv_obj_invalidate_area(s_synth_backdrop, &grid_area);
    }

    brush_tween_value(&s_scene.clock, now_ms, &g);
    const lv_coord_t h = (lv_coord_t)lroundf(g.r);
    const lv_coord_t top = (lv_coord_t)lroundf(g.cy - g.r / 2);
    if (h != s_clock_h) {
        s_clock_h = h;
        seg_set_height(s_clock, h);
    }
    const lv_coord_t clock_y = top + shift_y;
    if (shift_x != s_clock_x || clock_y != s_clock_y || top != s_clock_top) {
        s_clock_x = shift_x;
        s_clock_y = clock_y;
        s_clock_top = top;
        lv_obj_align(s_clock, LV_ALIGN_TOP_MID, shift_x, clock_y);
    }
    const lv_coord_t date_y = top + h + DATE_GAP + shift_y;
    if (shift_x != s_date_x || date_y != s_date_y) {
        s_date_x = shift_x;
        s_date_y = date_y;
        lv_obj_align(s_date, LV_ALIGN_TOP_MID, shift_x, date_y);
    }
    if (face != s_hint_face) {
        s_hint_face = face;
        lv_obj_set_style_text_font(s_hint, face == CLOCK_FACE_CLASSIC ? &tb_font_20 : &tb_font_14, 0);
    }
    const FacePalette &palette = kFacePalettes[face], &classic = kFacePalettes[CLOCK_FACE_CLASSIC];
    const lv_coord_t hint_y = palette.hint_top + shift_y;
    if (shift_x != s_hint_x || hint_y != s_hint_y) {
        s_hint_x = shift_x;
        s_hint_y = hint_y;
        lv_obj_align(s_hint, LV_ALIGN_TOP_MID, shift_x, hint_y);
    }

    s_idle_opa = opa;
    const auto themed = [&](uint32_t FacePalette::*field) {
        return lv_color_mix(lv_color_hex(palette.*field), lv_color_hex(classic.*field), theme_opa);
    };
    const lv_color_t clock_color = lv_color_mix(themed(&FacePalette::clock), lv_color_hex(COLOR_MUTED), opa);
    const lv_color_t clock_final = lv_color_mix(clock_color, accent, (lv_opa_t)(LV_OPA_COVER - saver_opa));
    seg_set_color(s_clock, clock_final);

    // Face fonts: the font clock is shown while idle (fading out as the segment clock shrinks into
    // the header when a timer starts); the date switches font directly.
    const FaceFonts fonts = face_fonts(face, custom_font);
    const lv_font_t *date_font = fonts.date ? fonts.date : &tb_font_28;
    if (lv_obj_get_style_text_font(s_date, LV_PART_MAIN) != date_font) {
        lv_obj_set_style_text_font(s_date, date_font, 0);
    }
    const lv_opa_t font_clock_opa = fonts.clock ? opa : LV_OPA_TRANSP;
    seg_set_opa(s_clock, (lv_opa_t)(LV_OPA_COVER - font_clock_opa));
    set_visible(s_font_clock, font_clock_opa > LV_OPA_MIN);
    if (fonts.clock) {
        if (fonts.clock != s_font_clock_font) {
            s_font_clock_font = fonts.clock;
            lv_obj_set_style_text_font(s_font_clock, fonts.clock, 0);
            s_font_clock_y = -1; // re-center for the new font
        }
        const lv_coord_t fy = font_clock_top(fonts.clock) + shift_y;
        if (shift_x != s_font_clock_x || fy != s_font_clock_y) {
            s_font_clock_x = shift_x;
            s_font_clock_y = fy;
            lv_obj_align(s_font_clock, LV_ALIGN_TOP_MID, shift_x, fy);
        }
        set_text_color(s_font_clock, clock_final);
        set_text_opa(s_font_clock, font_clock_opa);
    }
    set_text_color(s_date,
        lv_color_mix(themed(&FacePalette::date), accent, (lv_opa_t)(LV_OPA_COVER - (uint16_t)saver_opa * 3 / 8)));
    set_text_opa(s_date, opa);
    const lv_opa_t hint_opa = theme_opa;
    const lv_color_t hint_color = themed(&FacePalette::hint);
    set_text_opa(s_hint, hint_opa);
    set_text_color(s_hint, hint_color);
    set_text_opa(s_gear_icon, hint_opa);
    set_text_color(s_gear_icon, hint_color);
    set_arc_style(s_seconds_arc, LV_PART_MAIN, themed(&FacePalette::track), ring_alpha);
    set_arc_style(s_seconds_arc, LV_PART_INDICATOR,
                  lv_color_mix(themed(&FacePalette::ring), accent, (lv_opa_t)(LV_OPA_COVER - saver_opa)), ring_alpha);
    const bool visible = opa > LV_OPA_MIN;
    set_visible(s_date, visible);
    set_visible(s_hint, visible && hint_opa > LV_OPA_MIN);
    set_visible(s_gear, visible && hint_opa > LV_OPA_MIN);
    if (hint_opa > LV_OPA_70 && s_core->count == 0) {
        lv_obj_add_flag(s_gear, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(s_gear, LV_OBJ_FLAG_CLICKABLE);
    }
    set_visible(s_seconds_arc, !flower && visible && ring_alpha > LV_OPA_MIN);
}

// ---- Timer rings ---------------------------------------------------------------------------

float clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float phase_of(uint32_t t, uint32_t start, uint32_t dur)
{
    return t <= start ? 0.0f : clamp01((float)(t - start) / dur);
}

// 0 -> 1 -> 0 every SMILEY_LOOP_MS from `start`.
float loop_wave(uint32_t t, uint32_t start)
{
    if (t < start) {
        return 0.0f;
    }
    const float p = (float)((t - start) % SMILEY_LOOP_MS) / SMILEY_LOOP_MS;
    return 0.5f - 0.5f * cosf(6.28318530718f * p);
}

// Draws in the preview's face units (ring outer radius SMILEY_UNIT) around the card center.
struct SmileyPen {
    lv_draw_ctx_t *ctx;
    lv_point_t center;
    float k; // pixels per unit

    lv_point_t at(float x, float y) const
    {
        return {(lv_coord_t)(center.x + lroundf(x * k)), (lv_coord_t)(center.y + lroundf(y * k))};
    }
    void polygon(const lv_point_t *points, uint16_t count, lv_color_t color, lv_opa_t opa) const
    {
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = color;
        dsc.bg_opa = opa;
        lv_draw_polygon(ctx, &dsc, points, count);
    }
    void dot(float x, float y, float r, lv_color_t color, lv_opa_t opa) const
    {
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = color;
        dsc.bg_opa = opa;
        dsc.radius = LV_RADIUS_CIRCLE;
        const lv_point_t p = at(x, y);
        const lv_coord_t pr = (lv_coord_t)LV_MAX(1, lroundf(r * k));
        const lv_area_t area = {(lv_coord_t)(p.x - pr), (lv_coord_t)(p.y - pr),
                                (lv_coord_t)(p.x + pr), (lv_coord_t)(p.y + pr)};
        lv_draw_rect(ctx, &dsc, &area);
    }
    void line(float x1, float y1, float x2, float y2, float width, lv_color_t color, lv_opa_t opa) const
    {
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = color;
        dsc.opa = opa;
        dsc.width = (lv_coord_t)LV_MAX(1, lroundf(width * k));
        dsc.round_start = 1;
        dsc.round_end = 1;
        const lv_point_t a = at(x1, y1), b = at(x2, y2);
        lv_draw_line(ctx, &dsc, &a, &b);
    }
    // `r` is the radius of the stroke's center line; angles in degrees, clockwise from 3 o'clock.
    void arc(float x, float y, float r, float width, float start, float end, lv_color_t color,
             lv_opa_t opa) const
    {
        if (end - start < 1.0f) {
            return;
        }
        lv_draw_arc_dsc_t dsc;
        lv_draw_arc_dsc_init(&dsc);
        dsc.color = color;
        dsc.opa = opa;
        dsc.width = (lv_coord_t)LV_MAX(1, lroundf(width * k));
        dsc.rounded = 1;
        const lv_point_t c = at(x, y);
        lv_draw_arc(ctx, &dsc, &c, (uint16_t)lroundf(r * k + dsc.width / 2.0f), (uint16_t)lroundf(start),
                    (uint16_t)lroundf(end));
    }
};

// Five-pointed star from convex pieces (LVGL polygons must be convex).
void draw_star(const SmileyPen &pen, float x, float y, float outer, float inner, float rot_deg,
               lv_color_t color, lv_opa_t opa)
{
    lv_point_t tips[5], notches[5], core[5];
    for (int i = 0; i < 5; i++) {
        const float a = (-90.0f + rot_deg + i * 72.0f) * 0.01745329252f;
        const float b = a + 36.0f * 0.01745329252f;
        // The core is 1 px larger than the notches so anti-aliased seams don't show.
        const float core_r = inner + 1.0f / pen.k;
        tips[i] = pen.at(x + outer * cosf(a), y + outer * sinf(a));
        notches[i] = pen.at(x + inner * cosf(b), y + inner * sinf(b));
        core[i] = pen.at(x + core_r * cosf(b), y + core_r * sinf(b));
    }
    pen.polygon(core, 5, color, opa);
    for (int i = 0; i < 5; i++) {
        const lv_point_t spike[3] = {notches[(i + 4) % 5], tips[i], notches[i]};
        pen.polygon(spike, 3, color, opa);
    }
}

void draw_sparkle(const SmileyPen &pen, float x, float y, float scale, lv_color_t color, lv_opa_t opa)
{
    const float l = 9.0f * scale, w = LV_MAX(2.0f * scale, 1.5f / pen.k);
    const lv_point_t vertical[4] = {pen.at(x, y - l), pen.at(x + w, y), pen.at(x, y + l), pen.at(x - w, y)};
    const lv_point_t horizontal[4] = {pen.at(x - l, y), pen.at(x, y - w), pen.at(x + l, y), pen.at(x, y + w)};
    pen.polygon(vertical, 4, color, opa);
    pen.polygon(horizontal, 4, color, opa);
}

constexpr float SMILEY_EYES[2][2] = {{-37.0f, -25.0f}, {37.0f, -25.0f}};
constexpr float SMILEY_SPARKLES[4][3] = {{-110.0f, -100.0f, 2000.0f}, {112.0f, -90.0f, 2500.0f},
                                         {105.0f, 100.0f, 3000.0f}, {-112.0f, 90.0f, 2300.0f}};

void draw_smiley(lv_event_t *e)
{
    const Card &c = *static_cast<const Card *>(lv_event_get_user_data(e));
    if (!c.smiley || c.opa <= LV_OPA_MIN) {
        return;
    }
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    const lv_area_t &box = c.face->coords;
    const uint32_t t = c.smiley_now - c.smiley_ms;
    const float bounce = t >= SMILEY_BOUNCE_MS && t < SMILEY_INTRO_MS
        ? 1.0f + 0.06f * sinf(3.14159265f * (t - SMILEY_BOUNCE_MS) / SMILEY_BOUNCE_DUR_MS)
        : 1.0f;
    const SmileyPen pen = {ctx, {(lv_coord_t)((box.x1 + box.x2 + 1) / 2), (lv_coord_t)((box.y1 + box.y2 + 1) / 2)},
                           c.r / SMILEY_UNIT * bounce};
    const lv_color_t color = lv_color_hex(c.color), gold = lv_color_hex(s_palette->ink ? 0xD99A1E : SMILEY_GOLD);

    // The four quadrant arcs close their gaps, fill up and contract into the head.
    const float m = brush_ease(EASE_IN_OUT_CUBIC, phase_of(t, SMILEY_MORPH_DELAY_MS, SMILEY_MORPH_MS));
    const float head_w = c.arc_w * 10.0f / 16.0f;
    lv_draw_arc_dsc_t ring;
    lv_draw_arc_dsc_init(&ring);
    ring.color = color;
    ring.rounded = 1;
    ring.width = (lv_coord_t)LV_MAX(1, lroundf(lerpf(c.arc_w, head_w, m) * bounce));
    const uint16_t outer = (uint16_t)lroundf(lerpf(c.r, 125.0f * c.r / SMILEY_UNIT + head_w / 2.0f, m) * bounce);
    if (m >= 1.0f) {
        ring.opa = c.opa;
        lv_draw_arc(ctx, &ring, &pen.center, outer, 0, 360);
    } else {
        const float gap = c.half_gap * (1.0f - m);
        const lv_opa_t track = scale_opa(to_opa(TRACK_OPA / 255.0f * (1.0f - m)), c.opa);
        for (int q = 0; q < BRUSH_QUADRANTS; q++) {
            const float start = 270.0f + q * 90.0f + gap, span = 90.0f - 2.0f * gap;
            if (track > LV_OPA_MIN) {
                ring.opa = track;
                lv_draw_arc(ctx, &ring, &pen.center, outer, (uint16_t)lroundf(start),
                            (uint16_t)lroundf(start + span));
            }
            const float fill = span * lerpf(c.quad_fill[q] / 1000.0f, 1.0f, m);
            if (fill >= 1.0f) {
                ring.opa = c.opa;
                lv_draw_arc(ctx, &ring, &pen.center, outer, (uint16_t)lroundf(start),
                            (uint16_t)lroundf(start + fill));
            }
        }
    }

    for (int i = 0; i < 2; i++) {
        const float pop = brush_ease(EASE_OUT_BACK, phase_of(t, SMILEY_EYE_MS[i], SMILEY_EYE_POP_MS));
        if (pop <= 0.01f) {
            continue;
        }
        const float x = SMILEY_EYES[i][0], y = SMILEY_EYES[i][1];
        if (c.rating >= 3) {
            const float twinkle = loop_wave(t, SMILEY_EYE_MS[i] + 600);
            const float s = pop * (1.0f + 0.18f * twinkle);
            draw_star(pen, x, y, 19.5f * s, 8.0f * s, 18.0f * twinkle, gold, c.opa);
        } else {
            pen.dot(x, y, 11.0f * pop, color, c.opa);
        }
    }

    const float draw = brush_ease(EASE_OUT_CUBIC, phase_of(t, SMILEY_MOUTH_MS, SMILEY_MOUTH_DRAW_MS));
    constexpr float mouth_w = 10.0f;
    if (draw > 0.0f) {
        if (c.rating == 0) {
            pen.line(-45.0f, 52.0f, -45.0f + 90.0f * draw, 52.0f, mouth_w, color, c.opa);
        } else if (c.rating == 1) {
            // Flat, then curling up on the right.
            constexpr float straight = 53.0f, curve = 36.0f * 1.5707963f;
            const float len = draw * (straight + curve);
            pen.line(-45.0f, 52.0f, -45.0f + LV_MIN(len, straight), 52.0f, mouth_w, color, c.opa);
            if (len > straight) {
                pen.arc(8.0f, 16.0f, 36.0f, mouth_w, 90.0f - 90.0f * (len - straight) / curve, 90.0f, color, c.opa);
            }
        } else {
            pen.arc(0.0f, -4.6f, 62.0f, mouth_w, 155.0f - 130.0f * draw, 155.0f, color, c.opa);
        }
    }

    if (c.rating >= 3) {
        for (const auto &s : SMILEY_SPARKLES) {
            const float wave = loop_wave(t, (uint32_t)s[2]);
            if (wave > 0.02f) {
                draw_sparkle(pen, s[0], s[1], 0.3f + 0.7f * wave, gold, scale_opa(to_opa(wave), c.opa));
            }
        }
    }
}

// After the intro only the twinkling stars and sparkles change; redraw just those.
void invalidate_smiley_loops(const Card &c)
{
    const lv_area_t &box = c.face->coords;
    const float k = c.r / SMILEY_UNIT;
    const lv_coord_t cx = (lv_coord_t)((box.x1 + box.x2 + 1) / 2), cy = (lv_coord_t)((box.y1 + box.y2 + 1) / 2);
    const auto invalidate = [&](float x, float y, float half) {
        const lv_coord_t h = (lv_coord_t)lroundf(half * k) + 2;
        const lv_coord_t px = (lv_coord_t)(cx + lroundf(x * k)), py = (lv_coord_t)(cy + lroundf(y * k));
        const lv_area_t area = {(lv_coord_t)(px - h), (lv_coord_t)(py - h), (lv_coord_t)(px + h), (lv_coord_t)(py + h)};
        lv_obj_invalidate_area(c.face, &area);
    };
    for (const auto &eye : SMILEY_EYES) {
        invalidate(eye[0], eye[1], 26.0f);
    }
    for (const auto &s : SMILEY_SPARKLES) {
        invalidate(s[0], s[1], 11.0f);
    }
}

void create_card(int color)
{
    Card &c = s_cards[color];
    c.obj = lv_obj_create(s_view);
    lv_obj_remove_style_all(c.obj);
    lv_obj_clear_flag(c.obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c.obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_radius(c.obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c.obj, lv_color_white(), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(c.obj, 20, LV_STATE_PRESSED);
    lv_obj_add_event_cb(c.obj, card_event_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)color);
    lv_obj_add_event_cb(c.obj, card_event_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)color);

    for (lv_obj_t *&arc : c.arcs) {
        arc = make_arc(c.obj);
    }
    c.digits = seg_create(c.obj, 40);
    seg_set_text(c.digits, "2:00");
    c.status = make_label(c.obj, &tb_font_14, COLOR_MUTED, "");
    c.pressure = make_arc(c.obj);
    lv_arc_set_bg_angles(c.pressure, 0, 360);
    lv_obj_set_style_arc_opa(c.pressure, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_add_flag(c.pressure, LV_OBJ_FLAG_HIDDEN);
    c.icon = brush_icon_create(c.obj);
    lv_obj_add_flag(c.icon, LV_OBJ_FLAG_HIDDEN);
    c.face = lv_obj_create(c.obj);
    lv_obj_remove_style_all(c.face);
    lv_obj_clear_flag(c.face, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(c.face, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(c.face, draw_smiley, LV_EVENT_DRAW_MAIN, &c);
    c.cx = c.cy = c.r = -1;
    c.arc_w = 6;
    c.half_gap = 0;
    c.opa = 0;
    c.content_opa = LV_OPA_COVER;
    c.state = BT_RUNNING;
    c.color = kTimerColors[color];
    c.oralb = false;
    c.pressure_high = false;
    c.smiley = false;
}

void style_card(Card &c)
{
    const bool paused = c.state == BT_PAUSED && !c.smiley;
    const lv_color_t color = lv_color_hex(c.color);
    const lv_color_t dimmed = paused ? lv_color_mix(color, lv_color_black(), LV_OPA_50) : color;
    const lv_opa_t indicator = scale_opa(paused ? LV_OPA_50 : LV_OPA_COVER, c.opa);
    const lv_opa_t content = scale_opa(c.content_opa, c.opa);
    for (lv_obj_t *arc : c.arcs) {
        set_visible(arc, !c.smiley);
        lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, scale_opa(TRACK_OPA, c.opa), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arc, indicator, LV_PART_INDICATOR);
    }
    seg_set_color(c.digits, dimmed);
    seg_set_opa(c.digits, content);
    set_visible(c.digits, content > LV_OPA_MIN);

    // Pressure is only meaningful while the brush is actually running.
    const bool too_hard = c.oralb && c.pressure_high && c.state == BT_RUNNING;
    const uint32_t ink = s_palette->ink; // light dial: white details and gray text use the face's ink
    lv_color_t status_color = c.state == BT_RUNNING ? lv_color_hex(ink ? ink : COLOR_MUTED) : color;
    if (too_hard) {
        status_color = lv_color_hex(COLOR_PRESSURE_HIGH);
    }
    lv_obj_set_style_text_color(c.status, status_color, 0);
    lv_obj_set_style_text_opa(c.status, content, 0);

    set_visible(c.icon, c.oralb && content > LV_OPA_MIN);
    set_visible(c.pressure, c.oralb && c.state != BT_DONE && !c.smiley);
    if (c.oralb) {
        const lv_color_t bristles = too_hard ? lv_color_hex(COLOR_PRESSURE_HIGH)
                                             : ink ? lv_color_hex(ink) : lv_color_white();
        brush_icon_set_colors(c.icon, dimmed, bristles, content);
        lv_obj_set_style_arc_color(c.pressure,
            lv_color_hex(too_hard ? COLOR_PRESSURE_HIGH : ink ? ink : COLOR_PRESSURE_OK), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(c.pressure, scale_opa(too_hard ? LV_OPA_COVER : PRESSURE_OK_OPA, c.opa),
                                 LV_PART_MAIN);
    }
    set_visible(c.face, c.smiley);
    if (c.smiley) {
        lv_obj_invalidate(c.face);
    }
}

void apply_card_geometry(Card &c, const brush_geom_t &g)
{
    const lv_opa_t opa = to_opa(g.opa);
    const lv_coord_t r = (lv_coord_t)lroundf(g.r);
    if (opa <= LV_OPA_MIN || r < 10) {
        set_visible(c.obj, false);
        c.opa = 0;
        return;
    }
    set_visible(c.obj, true);

    const lv_coord_t cx = (lv_coord_t)lroundf(g.cx), cy = (lv_coord_t)lroundf(g.cy);
    if (r != c.r) {
        const lv_coord_t size = r * 2;
        const lv_coord_t arc_w = LV_MAX(6, r * 85 / 1000);
        // Four quadrant arcs starting at 12 o'clock; the rounded caps need a bit of extra gap.
        const int half_gap = 2 + (arc_w * 2865 / 100 + r / 2) / r;
        lv_obj_set_size(c.obj, size, size);
        lv_obj_set_size(c.face, size, size);
        c.arc_w = arc_w;
        c.half_gap = half_gap;
        for (int q = 0; q < BRUSH_QUADRANTS; q++) {
            lv_obj_set_size(c.arcs[q], size, size);
            lv_obj_set_style_arc_width(c.arcs[q], arc_w, LV_PART_MAIN);
            lv_obj_set_style_arc_width(c.arcs[q], arc_w, LV_PART_INDICATOR);
            lv_arc_set_bg_angles(c.arcs[q], (uint16_t)((270 + q * 90 + half_gap) % 360),
                                 (uint16_t)((270 + (q + 1) * 90 - half_gap) % 360));
        }
        const lv_coord_t digit_h = r * 46 / 100;
        const lv_coord_t digits_y = -r * 6 / 100;
        seg_set_height(c.digits, digit_h);
        lv_obj_align(c.digits, LV_ALIGN_CENTER, 0, digits_y);
        lv_obj_set_style_text_font(c.status, r >= 120 ? &tb_font_20 : &tb_font_14, 0);
        lv_obj_align(c.status, LV_ALIGN_CENTER, 0, digits_y + digit_h / 2 + r * 18 / 100);

        // Toothbrush sits between the top of the ring and the digits; pressure ring just inside the arcs.
        brush_icon_set_width(c.icon, r * 46 / 100);
        lv_obj_align(c.icon, LV_ALIGN_CENTER, 0, -r * 48 / 100);
        const lv_coord_t pr = r - arc_w - LV_MAX(4, r * 5 / 100);
        lv_obj_set_size(c.pressure, pr * 2, pr * 2);
        lv_obj_center(c.pressure);
        lv_obj_set_style_arc_width(c.pressure, LV_MAX(3, r * 25 / 1000), LV_PART_MAIN);
    }
    if (r != c.r || cx != c.cx || cy != c.cy) {
        lv_obj_set_pos(c.obj, cx - r, cy - r);
        c.cx = cx;
        c.cy = cy;
        c.r = r;
    }
    if (opa != c.opa) {
        c.opa = opa;
        style_card(c);
    }
}

void update_smiley(Card &c, uint32_t now_ms)
{
    c.smiley_now = now_ms;
    const uint32_t t = now_ms - c.smiley_ms;
    const lv_opa_t content = t >= SMILEY_CONTENT_OUT_MS
        ? LV_OPA_TRANSP : to_opa(1.0f - (float)t / SMILEY_CONTENT_OUT_MS);
    if (content != c.content_opa) {
        c.content_opa = content;
        style_card(c); // also redraws the face
    } else if (t < SMILEY_INTRO_MS + 100) {
        lv_obj_invalidate(c.face);
    } else if (c.rating >= 3) {
        invalidate_smiley_loops(c);
    }
}

void update_card_content(Card &c, int index, uint32_t now_ms, bool count_up)
{
    const brush_timer_t &t = s_core->timers[index];
    const uint32_t color = card_color(kTimerColors[t.color % BRUSH_MAX_TIMERS]);
    const bool oralb = t.source == BRUSH_SRC_ORALB;
    const bool pressure_high = oralb && t.pressure_high;
    const bool smiley = t.state == BT_DONE || t.finishing;
    if (pressure_high && !c.pressure_high && t.state == BT_RUNNING) {
        brush_scene_pulse(&s_scene, t.color, PULSE_PRESSURE_AMP, PULSE_PRESSURE_MS, now_ms);
    }
    if (smiley != c.smiley) {
        c.smiley = smiley;
        c.content_opa = LV_OPA_COVER;
        if (smiley) {
            c.smiley_ms = c.smiley_now = now_ms;
            c.rating = brush_rating(s_core, index, now_ms);
            for (int q = 0; q < BRUSH_QUADRANTS; q++) {
                c.quad_fill[q] = brush_quadrant_permille(s_core, index, q, now_ms);
            }
        }
        style_card(c);
    }
    if (c.state != t.state || c.color != color || c.oralb != oralb || c.pressure_high != pressure_high) {
        c.state = (int8_t)t.state;
        c.color = color;
        c.oralb = oralb;
        c.pressure_high = pressure_high;
        style_card(c);
    }
    if (c.smiley) {
        update_smiley(c, now_ms);
        return;
    }

    for (int q = 0; q < BRUSH_QUADRANTS; q++) {
        lv_arc_set_value(c.arcs[q], (int16_t)brush_quadrant_permille(s_core, index, q, now_ms));
    }

    char buf[8];
    brush_format_mmss(brush_display_seconds(s_core, index, now_ms, count_up, nullptr), buf);
    seg_set_text(c.digits, buf);

    const char *status = "";
    switch (t.state) {
    case BT_PAUSED: status = s_txt->paused; break;
    case BT_DONE: status = s_txt->done; break;
    case BT_RUNNING:
        status = (oralb && t.pressure_high) ? s_txt->too_hard : ""; // quadrant names hidden for now
        break;
    }
    if (strcmp(lv_label_get_text(c.status), status) != 0) {
        lv_label_set_text(c.status, status);
    }
}

// ---- "+" button ----------------------------------------------------------------------------

void build_add_button()
{
    s_add = lv_obj_create(s_view);
    lv_obj_remove_style_all(s_add);
    lv_obj_clear_flag(s_add, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_add, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_radius(s_add, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(s_add, lv_color_hex(COLOR_OUTLINE), 0);
    lv_obj_add_event_cb(s_add, add_event_cb, LV_EVENT_CLICKED, nullptr);
    s_add_plus = make_label(s_add, &tb_font_28, COLOR_MUTED, LV_SYMBOL_PLUS);
    lv_obj_center(s_add_plus);
}

void style_add_button()
{
    const lv_opa_t opa = s_add_opa;
    if (s_add_ghost) {
        lv_obj_set_style_bg_opa(s_add, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_add, 3, 0);
        lv_obj_set_style_border_opa(s_add, opa, 0);
        lv_obj_set_style_bg_color(s_add, lv_color_white(), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(s_add, scale_opa(20, opa), LV_STATE_PRESSED);
    } else {
        lv_obj_set_style_border_width(s_add, 0, 0);
        lv_obj_set_style_bg_color(s_add, lv_color_hex(COLOR_SURFACE), 0);
        lv_obj_set_style_bg_opa(s_add, opa, 0);
        lv_obj_set_style_bg_color(s_add, lv_color_hex(COLOR_SURFACE_PRESSED), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(s_add, opa, LV_STATE_PRESSED);
    }
    lv_obj_set_style_text_opa(s_add_plus, opa, 0);
}

void apply_add(uint32_t now_ms)
{
    brush_geom_t g;
    brush_tween_value(&s_scene.add, now_ms, &g);
    const lv_opa_t opa = to_opa(g.opa);
    const lv_coord_t r = (lv_coord_t)lroundf(g.r);
    const bool visible = opa > LV_OPA_MIN && r > 0;
    set_visible(s_add, visible);
    if (!visible) {
        return;
    }
    const lv_coord_t cx = (lv_coord_t)lroundf(g.cx), cy = (lv_coord_t)lroundf(g.cy);
    if (cx != s_add_cx || cy != s_add_cy || r != s_add_r) {
        s_add_cx = cx;
        s_add_cy = cy;
        s_add_r = r;
        lv_obj_set_size(s_add, r * 2, r * 2);
        lv_obj_set_pos(s_add, cx - r, cy - r);
    }
    if (opa != s_add_opa) {
        s_add_opa = opa;
        style_add_button();
    }
    // Only tappable once it has (almost) fully appeared.
    if (opa > LV_OPA_70) {
        lv_obj_add_flag(s_add, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(s_add, LV_OBJ_FLAG_CLICKABLE);
    }
}

void on_layout_changed()
{
    brush_layout_t l;
    brush_layout(s_core->count, &l);
    if (l.add_is_ghost != s_add_ghost) {
        s_add_ghost = l.add_is_ghost;
        style_add_button();
    }
    if (s_core->count == 0) {
        lv_obj_add_flag(s_view, LV_OBJ_FLAG_CLICKABLE);
    } else {
        lv_obj_clear_flag(s_view, LV_OBJ_FLAG_CLICKABLE);
    }
}

// ---- Settings screen --------------------------------------------------------------------

void show_screen(lv_obj_t *scr) { lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_FADE_ON, 220, 0, false); }

void refresh_settings()
{
    const brush_strings_t *t = brush_strings(s_set_values.lang);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", s_set_hour, s_set_minute);
    seg_set_text(s_set_digits, buf);
    lv_label_set_text(s_set_title, t->settings);
    lv_label_set_text_fmt(s_mode_label, "%s %s", s_set_values.count_up ? LV_SYMBOL_UP : LV_SYMBOL_DOWN,
                          s_set_values.count_up ? t->count_up : t->countdown);
    lv_label_set_text_fmt(s_sound_label, "%s %s", s_set_values.sound ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE,
                          s_set_values.sound ? t->sound : t->muted);
    lv_label_set_text_fmt(s_bright_label, "%s %d%%", LV_SYMBOL_EYE_OPEN, s_set_values.brightness);
    lv_label_set_text(s_lang_label, t->language);
    const char *face = s_set_values.clock_face == CLOCK_FACE_CANDY ? t->candy
                     : s_set_values.clock_face == CLOCK_FACE_HEART ? t->heart
                     : s_set_values.clock_face == CLOCK_FACE_REINDEER ? t->reindeer
                     : s_set_values.clock_face == CLOCK_FACE_NINETIES ? "90s"
                     : s_set_values.clock_face == CLOCK_FACE_CHRISTMAS ? t->christmas
                     : s_set_values.clock_face == CLOCK_FACE_FLOWER_POWER ? "Flower Power"
                     : s_set_values.clock_face == CLOCK_FACE_SYNTHWAVE ? "Synthwave" : t->classic;
    lv_label_set_text_fmt(s_face_label, "%s: %s", t->clock_face, face);
    lv_label_set_text_fmt(s_font_label, "Aa %s", s_set_values.custom_font ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
    lv_label_set_text_fmt(s_duration_label, "%s %d:%02d", LV_SYMBOL_BELL, s_set_values.duration_s / 60,
                          s_set_values.duration_s % 60);
    lv_label_set_text(s_ok_label, t->ok);
}

enum SettingsAction : intptr_t {
    HOUR_UP, HOUR_DOWN, MIN_UP, MIN_DOWN, TOGGLE_MODE, TOGGLE_SOUND, TOGGLE_BRIGHTNESS, TOGGLE_FACE, TOGGLE_FONT,
    TOGGLE_DURATION, TOGGLE_LANG, DONE,
};

void settings_event_cb(lv_event_t *e)
{
    const SettingsAction action = (SettingsAction)(intptr_t)lv_event_get_user_data(e);
    switch (action) {
    case HOUR_UP: s_set_hour = (s_set_hour + 1) % 24; s_time_changed = true; break;
    case HOUR_DOWN: s_set_hour = (s_set_hour + 23) % 24; s_time_changed = true; break;
    case MIN_UP: s_set_minute = (s_set_minute + 1) % 60; s_time_changed = true; break;
    case MIN_DOWN: s_set_minute = (s_set_minute + 59) % 60; s_time_changed = true; break;
    case TOGGLE_MODE: s_set_values.count_up = !s_set_values.count_up; break;
    case TOGGLE_SOUND: s_set_values.sound = !s_set_values.sound; break;
    case TOGGLE_BRIGHTNESS:
        s_set_values.brightness = s_set_values.brightness >= 100 ? 25 : (uint8_t)(s_set_values.brightness / 25 * 25 + 25);
        app_preview_brightness(s_set_values.brightness);
        break;
    case TOGGLE_FONT: s_set_values.custom_font = !s_set_values.custom_font; break;
    case TOGGLE_DURATION: {
        size_t i = 0;
        while (i < LV_ARRAY_LEN(kDurations) && kDurations[i] != s_set_values.duration_s) {
            i++;
        }
        s_set_values.duration_s = kDurations[(i + 1) % LV_ARRAY_LEN(kDurations)]; // unknown values restart at 2:00
        break;
    }
    case TOGGLE_FACE:
        s_set_values.clock_face = static_cast<ClockFace>((s_set_values.clock_face + 1) % CLOCK_FACE_COUNT);
        break;
    case TOGGLE_LANG: s_set_values.lang = (brush_lang_t)((s_set_values.lang + 1) % BRUSH_LANG_COUNT); break;
    case DONE:
        app_settings_done(s_set_hour, s_set_minute, s_time_changed, s_set_values);
        show_screen(s_main_scr);
        return;
    }
    refresh_settings();
}

lv_obj_t *make_button(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, uint32_t bg, SettingsAction action,
                      bool repeat)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_color(btn, lv_color_mix(lv_color_hex(bg), lv_color_white(), LV_OPA_80), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, settings_event_cb, LV_EVENT_SHORT_CLICKED, (void *)(intptr_t)action);
    if (repeat) {
        lv_obj_add_event_cb(btn, settings_event_cb, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)action);
    } else {
        lv_obj_add_event_cb(btn, settings_event_cb, LV_EVENT_LONG_PRESSED, (void *)(intptr_t)action);
    }
    return btn;
}

void build_settings()
{
    s_settings_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_settings_scr, lv_color_black(), 0);
    lv_obj_clear_flag(s_settings_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_set_title = make_label(s_settings_scr, &tb_font_20, COLOR_MUTED, "");
    lv_obj_align(s_set_title, LV_ALIGN_TOP_MID, 0, 48);

    // "HH:MM" at 72 px: digits 37 px wide, 11 px apart, colon 7 px -> 199 px total.
    s_set_digits = seg_create(s_settings_scr, 72);
    seg_set_color(s_set_digits, lv_color_hex(COLOR_TEXT));
    seg_set_text(s_set_digits, "00:00");
    lv_obj_align(s_set_digits, LV_ALIGN_TOP_MID, 0, 144);

    const lv_coord_t hours_cx = 182, minutes_cx = 298;
    const struct {
        lv_coord_t cx, cy;
        const char *symbol;
        SettingsAction action;
    } arrows[] = {
        {hours_cx, 108, LV_SYMBOL_UP, HOUR_UP},
        {hours_cx, 252, LV_SYMBOL_DOWN, HOUR_DOWN},
        {minutes_cx, 108, LV_SYMBOL_UP, MIN_UP},
        {minutes_cx, 252, LV_SYMBOL_DOWN, MIN_DOWN},
    };
    for (const auto &a : arrows) {
        lv_obj_t *btn = make_button(s_settings_scr, 72, 44, COLOR_SURFACE, a.action, true);
        lv_obj_set_pos(btn, a.cx - 36, a.cy - 22);
        lv_obj_center(make_label(btn, &tb_font_20, COLOR_TEXT, a.symbol));
    }

    // Mode | sound | brightness share one row.
    lv_obj_t *mode = make_button(s_settings_scr, 140, 44, COLOR_SURFACE, TOGGLE_MODE, false);
    lv_obj_set_pos(mode, 44, 286);
    s_mode_label = make_label(mode, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_mode_label);

    lv_obj_t *sound = make_button(s_settings_scr, 128, 44, COLOR_SURFACE, TOGGLE_SOUND, false);
    lv_obj_set_pos(sound, 192, 286);
    s_sound_label = make_label(sound, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_sound_label);

    lv_obj_t *bright = make_button(s_settings_scr, 108, 44, COLOR_SURFACE, TOGGLE_BRIGHTNESS, false);
    lv_obj_set_pos(bright, 328, 286);
    s_bright_label = make_label(bright, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_bright_label);

    lv_obj_t *face = make_button(s_settings_scr, 240, 44, COLOR_SURFACE, TOGGLE_FACE, false);
    lv_obj_set_pos(face, 60, 338);
    s_face_label = make_label(face, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_face_label);

    // Custom face fonts on/off ("Aa ✓" / "Aa ✕").
    lv_obj_t *font = make_button(s_settings_scr, 112, 44, COLOR_SURFACE, TOGGLE_FONT, false);
    lv_obj_set_pos(font, 308, 338);
    s_font_label = make_label(font, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_font_label);

    // Brushing time | language | done; narrower buttons so the row fits inside the round screen.
    lv_obj_t *duration = make_button(s_settings_scr, 90, 44, COLOR_SURFACE, TOGGLE_DURATION, false);
    lv_obj_set_pos(duration, 99, 390);
    s_duration_label = make_label(duration, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_duration_label);

    lv_obj_t *lang = make_button(s_settings_scr, 90, 44, COLOR_SURFACE, TOGGLE_LANG, false);
    lv_obj_set_pos(lang, 195, 390);
    s_lang_label = make_label(lang, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_lang_label);

    lv_obj_t *done = make_button(s_settings_scr, 90, 44, 0x1F8A4C, DONE, false);
    lv_obj_set_pos(done, 291, 390);
    s_ok_label = make_label(done, &tb_font_20, 0xFFFFFF, "");
    lv_obj_center(s_ok_label);
}

} // namespace

void ui_init(brush_core_t *core, uint32_t now_ms)
{
    s_core = core;
    brush_scene_init(&s_scene, core, now_ms);

    s_main_scr = lv_scr_act();
    lv_obj_set_style_bg_color(s_main_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_main_scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_main_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_view = lv_obj_create(s_main_scr);
    lv_obj_remove_style_all(s_view);
    lv_obj_clear_flag(s_view, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_size(s_view, LCD_WIDTH, LCD_HEIGHT);
    for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_LONG_PRESSED, LV_EVENT_RELEASED}) {
        lv_obj_add_event_cb(s_view, idle_event_cb, code, nullptr);
    }

    build_idle();
    for (int c = 0; c < BRUSH_MAX_TIMERS; c++) {
        create_card(c);
    }
    build_add_button();
    style_add_button();
    on_layout_changed();
    build_settings();
}

void ui_update(uint32_t now_ms, const struct tm *local_time, uint16_t millis_in_second,
               const UiSettings &settings, bool screensaver)
{
    if (lv_scr_act() != s_main_scr) {
        return;
    }
    if (s_txt != brush_strings(settings.lang)) {
        s_txt = brush_strings(settings.lang);
        lv_label_set_text(s_hint, s_txt->hint);
    }
    if (brush_scene_sync(&s_scene, s_core, now_ms)) {
        on_layout_changed();
    }
    s_palette = &kFacePalettes[settings.clock_face < CLOCK_FACE_COUNT ? settings.clock_face : CLOCK_FACE_CLASSIC];

    for (int c = 0; c < BRUSH_MAX_TIMERS; c++) {
        Card &card = s_cards[c];
        brush_geom_t g;
        brush_scene_card(&s_scene, (uint8_t)c, now_ms, &g);
        apply_card_geometry(card, g);
        const int index = index_of_color(c);
        if (index >= 0) {
            lv_obj_add_flag(card.obj, LV_OBJ_FLAG_CLICKABLE);
            if (card.opa > 0) {
                update_card_content(card, index, now_ms, settings.count_up);
            }
        } else {
            lv_obj_clear_flag(card.obj, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    apply_add(now_ms);
    apply_idle(now_ms, screensaver, settings.clock_face, LV_MIN(local_time->tm_sec, FLOWER_MARKS - 1),
               settings.custom_font);
    update_birthday_flags(now_ms, *local_time);

    char clock[8];
    snprintf(clock, sizeof(clock), "%02d:%02d", local_time->tm_hour, local_time->tm_min);
    seg_set_text(s_clock, clock);
    if (strcmp(lv_label_get_text(s_font_clock), clock) != 0) {
        lv_label_set_text(s_font_clock, clock);
    }

    if (s_idle_opa > LV_OPA_MIN) {
        if (settings.clock_face != CLOCK_FACE_FLOWER_POWER) {
            // Clearing a full ring in one step redraws the whole screen, which the panel shows as a
            // partial wipe; erase the previous minute clockwise during second 0 instead.
            const int sec = local_time->tm_sec;
            const uint16_t start = sec == 0 ? (uint16_t)(millis_in_second * 360 / 1000) : 0;
            const uint16_t end = sec == 0 ? 360 : (uint16_t)((sec * 10 + millis_in_second / 100) * 360 / 600);
            lv_arc_set_angles(s_seconds_arc, start, end);
        }
        char date[40];
        brush_format_date(settings.lang, local_time->tm_wday, local_time->tm_mday, local_time->tm_mon, date,
                          sizeof(date));
        if (strcmp(date, s_date_text) != 0) {
            strcpy(s_date_text, date);
            lv_label_set_text(s_date, date);
        }
    }
}

bool ui_animating(uint32_t now_ms)
{
    if (lv_scr_act() == s_main_scr) {
        for (const Card &c : s_cards) {
            if (c.smiley && c.opa > LV_OPA_MIN && now_ms - c.smiley_ms < SMILEY_INTRO_MS) {
                return true;
            }
        }
    }
    return brush_scene_animating(&s_scene, now_ms) ||
           (lv_scr_act() == s_main_scr && s_saver_transitioning);
}

void ui_timer_event(const brush_event_t &event, uint32_t now_ms)
{
    if (event.type == BEV_MARK) {
        brush_scene_pulse(&s_scene, event.color, PULSE_MARK_AMP, PULSE_MARK_MS, now_ms);
    } else if (event.type == BEV_DONE) {
        brush_scene_pulse(&s_scene, event.color, PULSE_DONE_AMP, PULSE_DONE_MS, now_ms);
    }
}

void ui_open_settings(const struct tm *local_time, const UiSettings &settings)
{
    s_set_hour = local_time->tm_hour;
    s_set_minute = local_time->tm_min;
    s_time_changed = false;
    s_set_values = settings;
    refresh_settings();
    show_screen(s_settings_scr);
}
