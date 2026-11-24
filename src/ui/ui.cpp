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
constexpr uint32_t COLOR_DONE = 0x4ADE80;
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

LV_FONT_DECLARE(tb_font_14)
LV_FONT_DECLARE(tb_font_20)
LV_FONT_DECLARE(tb_font_28)

const brush_strings_t *s_txt = brush_strings(BRUSH_LANG_EN); // active language

// One ring per timer color slot; persistent so it can animate between layouts.
struct Card {
    lv_obj_t *obj;
    lv_obj_t *arcs[BRUSH_QUADRANTS];
    lv_obj_t *digits;
    lv_obj_t *status;
    lv_obj_t *icon;     // toothbrush, Oral-B timers only
    lv_obj_t *pressure; // inner ring, Oral-B timers only
    lv_coord_t cx, cy, r; // last applied geometry
    lv_opa_t opa;
    int8_t state;   // last shown timer state (kept while fading out)
    uint32_t color; // last shown color
    bool oralb;     // last shown source
    bool pressure_high;
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
ClockFace s_hint_face = CLOCK_FACE_CLASSIC;
lv_obj_t *s_clock = nullptr;
lv_coord_t s_clock_h = -1, s_clock_top = -1;
lv_obj_t *s_seconds_arc = nullptr;
lv_obj_t *s_date = nullptr;
lv_obj_t *s_hint = nullptr;
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
int s_set_hour = 0;
int s_set_minute = 0;
bool s_time_changed = false;
UiSettings s_set_values = {false, true, BRUSH_LANG_EN, CLOCK_FACE_CLASSIC};
lv_obj_t *s_set_title = nullptr;
lv_obj_t *s_lang_label = nullptr;
lv_obj_t *s_face_label = nullptr;
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

void idle_event_cb(lv_event_t *e)
{
    if (s_core->count != 0) {
        return;
    }
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED) {
        app_start_timer();
    } else if (code == LV_EVENT_LONG_PRESSED) {
        app_open_settings();
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
    if (s_synth_opa <= LV_OPA_MIN) {
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
    const int sweep = lv_arc_get_angle_end(s_seconds_arc);
    constexpr int step = 5;
    arc.rounded = false;
    const lv_area_t &clip = *part->draw_ctx->clip_area;
    for (int start = 0; start < sweep; start += step) {
        const int end = LV_MIN(start + step, sweep);
        lv_area_t bounds;
        lv_draw_arc_get_area(part->p1->x, part->p1->y, part->radius, 270 + start, 270 + end,
                             arc.width, true, &bounds);
        if (bounds.x2 < clip.x1 || bounds.x1 > clip.x2 || bounds.y2 < clip.y1 || bounds.y1 > clip.y2) {
            continue;
        }
        // Sample the fixed palette, not the length of the visible sweep.
        arc.color = synth_palette_color(kSynthRingColors, (start + step * 0.5f) / 360.0f);
        lv_draw_arc(part->draw_ctx, &arc, part->p1, part->radius, 270 + start, 270 + end);
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

void build_idle()
{
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

    s_clock = seg_create(s_view, 100);
    seg_set_text(s_clock, "00:00");
    build_birthday_flags();
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

void apply_idle(uint32_t now_ms, bool screensaver, ClockFace face, int second)
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
    const lv_coord_t hint_top = flower ? FLOWER_HINT_TOP : christmas ? CHRISTMAS_HINT_TOP
                             : face == CLOCK_FACE_SYNTHWAVE ? SYNTH_HINT_TOP : 372;
    const lv_coord_t hint_y = hint_top + shift_y;
    if (shift_x != s_hint_x || hint_y != s_hint_y) {
        s_hint_x = shift_x;
        s_hint_y = hint_y;
        lv_obj_align(s_hint, LV_ALIGN_TOP_MID, shift_x, hint_y);
    }

    s_idle_opa = opa;
    const auto themed = [&](uint32_t classic, uint32_t neon, uint32_t floral, uint32_t winter) {
        return lv_color_mix(lv_color_hex(christmas ? winter : flower ? floral : neon),
                            lv_color_hex(classic), christmas || flower ? theme_opa : synth);
    };
    const lv_color_t clock_color = lv_color_mix(themed(COLOR_TEXT, SYNTH_CLOCK, FLOWER_INK, CHRISTMAS_SNOW),
                                               lv_color_hex(COLOR_MUTED), opa);
    seg_set_color(s_clock, lv_color_mix(clock_color, accent, (lv_opa_t)(LV_OPA_COVER - saver_opa)));
    lv_obj_set_style_text_color(s_date,
        lv_color_mix(themed(COLOR_MUTED, SYNTH_DATE, 0x6C304A, CHRISTMAS_GOLD), accent,
                     (lv_opa_t)(LV_OPA_COVER - (uint16_t)saver_opa * 3 / 8)), 0);
    lv_obj_set_style_text_opa(s_date, opa, 0);
    const lv_opa_t hint_opa = theme_opa;
    lv_obj_set_style_text_opa(s_hint, hint_opa, 0);
    lv_obj_set_style_text_color(s_hint, themed(COLOR_FAINT, SYNTH_HINT, 0x3D5C4E, 0xA6C3B4), 0);
    lv_obj_set_style_arc_color(s_seconds_arc, themed(0x1A1A1C, SYNTH_TRACK, FLOWER_INK, CHRISTMAS_TRACK),
                              LV_PART_MAIN);
    const lv_color_t seconds_color = lv_color_mix(lv_color_hex(CHRISTMAS_GOLD),
                                                lv_color_hex(kTimerColors[0]), christmas_opa);
    lv_obj_set_style_arc_color(s_seconds_arc,
        lv_color_mix(seconds_color, accent, (lv_opa_t)(LV_OPA_COVER - saver_opa)),
        LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_seconds_arc, ring_alpha, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(s_seconds_arc, ring_alpha, LV_PART_INDICATOR);
    const bool visible = opa > LV_OPA_MIN;
    set_visible(s_date, visible);
    set_visible(s_hint, visible && hint_opa > LV_OPA_MIN);
    set_visible(s_seconds_arc, !flower && visible && ring_alpha > LV_OPA_MIN);
}

// ---- Timer rings ---------------------------------------------------------------------------

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
    c.cx = c.cy = c.r = -1;
    c.opa = 0;
    c.state = BT_RUNNING;
    c.color = kTimerColors[color];
    c.oralb = false;
    c.pressure_high = false;
}

void style_card(Card &c)
{
    const bool paused = c.state == BT_PAUSED;
    const lv_color_t color = lv_color_hex(c.color);
    const lv_color_t dimmed = paused ? lv_color_mix(color, lv_color_black(), LV_OPA_50) : color;
    const lv_opa_t indicator = scale_opa(paused ? LV_OPA_50 : LV_OPA_COVER, c.opa);
    for (lv_obj_t *arc : c.arcs) {
        lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
        lv_obj_set_style_arc_opa(arc, scale_opa(TRACK_OPA, c.opa), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(arc, indicator, LV_PART_INDICATOR);
    }
    seg_set_color(c.digits, dimmed);
    seg_set_opa(c.digits, c.opa);

    // Pressure is only meaningful while the brush is actually running.
    const bool too_hard = c.oralb && c.pressure_high && c.state == BT_RUNNING;
    lv_color_t status_color = c.state == BT_RUNNING ? lv_color_hex(COLOR_MUTED) : color;
    if (too_hard) {
        status_color = lv_color_hex(COLOR_PRESSURE_HIGH);
    }
    lv_obj_set_style_text_color(c.status, status_color, 0);
    lv_obj_set_style_text_opa(c.status, c.opa, 0);

    set_visible(c.icon, c.oralb);
    set_visible(c.pressure, c.oralb && c.state != BT_DONE);
    if (c.oralb) {
        const lv_color_t bristles = too_hard ? lv_color_hex(COLOR_PRESSURE_HIGH) : lv_color_white();
        brush_icon_set_colors(c.icon, dimmed, bristles, c.opa);
        lv_obj_set_style_arc_color(c.pressure, lv_color_hex(too_hard ? COLOR_PRESSURE_HIGH : COLOR_PRESSURE_OK),
                                   LV_PART_MAIN);
        lv_obj_set_style_arc_opa(c.pressure, scale_opa(too_hard ? LV_OPA_COVER : PRESSURE_OK_OPA, c.opa),
                                 LV_PART_MAIN);
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

void update_card_content(Card &c, int index, uint32_t now_ms, bool count_up)
{
    const brush_timer_t &t = s_core->timers[index];
    const uint32_t color = t.state == BT_DONE ? COLOR_DONE : kTimerColors[t.color % BRUSH_MAX_TIMERS];
    const bool oralb = t.source == BRUSH_SRC_ORALB;
    const bool pressure_high = oralb && t.pressure_high;
    if (pressure_high && !c.pressure_high && t.state == BT_RUNNING) {
        brush_scene_pulse(&s_scene, t.color, PULSE_PRESSURE_AMP, PULSE_PRESSURE_MS, now_ms);
    }
    if (c.state != t.state || c.color != color || c.oralb != oralb || c.pressure_high != pressure_high) {
        c.state = (int8_t)t.state;
        c.color = color;
        c.oralb = oralb;
        c.pressure_high = pressure_high;
        style_card(c);
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
        status = (oralb && t.pressure_high) ? s_txt->too_hard
                                            : s_txt->quadrants[brush_current_quadrant(s_core, index, now_ms)];
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
    lv_label_set_text_fmt(s_mode_label, "%s  %s", s_set_values.count_up ? LV_SYMBOL_UP : LV_SYMBOL_DOWN,
                          s_set_values.count_up ? t->count_up : t->countdown);
    lv_label_set_text_fmt(s_sound_label, "%s  %s", s_set_values.sound ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE,
                          s_set_values.sound ? t->sound : t->muted);
    lv_label_set_text(s_lang_label, t->language);
    const char *face = s_set_values.clock_face == CLOCK_FACE_CHRISTMAS ? t->christmas
                     : s_set_values.clock_face == CLOCK_FACE_FLOWER_POWER ? "Flower Power"
                     : s_set_values.clock_face == CLOCK_FACE_SYNTHWAVE ? "Synthwave" : t->classic;
    lv_label_set_text_fmt(s_face_label, "%s: %s", t->clock_face, face);
    lv_label_set_text(s_ok_label, t->ok);
}

enum SettingsAction : intptr_t {
    HOUR_UP, HOUR_DOWN, MIN_UP, MIN_DOWN, TOGGLE_MODE, TOGGLE_SOUND, TOGGLE_FACE, TOGGLE_LANG, DONE,
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

    lv_obj_t *mode = make_button(s_settings_scr, 168, 44, COLOR_SURFACE, TOGGLE_MODE, false);
    lv_obj_set_pos(mode, 240 - 8 - 168, 286);
    s_mode_label = make_label(mode, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_mode_label);

    lv_obj_t *sound = make_button(s_settings_scr, 168, 44, COLOR_SURFACE, TOGGLE_SOUND, false);
    lv_obj_set_pos(sound, 240 + 8, 286);
    s_sound_label = make_label(sound, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_sound_label);

    lv_obj_t *face = make_button(s_settings_scr, 252, 44, COLOR_SURFACE, TOGGLE_FACE, false);
    lv_obj_set_pos(face, 114, 338);
    s_face_label = make_label(face, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_face_label);

    lv_obj_t *lang = make_button(s_settings_scr, 118, 44, COLOR_SURFACE, TOGGLE_LANG, false);
    lv_obj_set_pos(lang, 114, 390);
    s_lang_label = make_label(lang, &tb_font_20, COLOR_TEXT, "");
    lv_obj_center(s_lang_label);

    lv_obj_t *done = make_button(s_settings_scr, 118, 44, 0x1F8A4C, DONE, false);
    lv_obj_set_pos(done, 248, 390);
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
    lv_obj_clear_flag(s_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_view, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_add_event_cb(s_view, idle_event_cb, LV_EVENT_SHORT_CLICKED, nullptr);
    lv_obj_add_event_cb(s_view, idle_event_cb, LV_EVENT_LONG_PRESSED, nullptr);

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
    apply_idle(now_ms, screensaver, settings.clock_face, LV_MIN(local_time->tm_sec, FLOWER_MARKS - 1));
    update_birthday_flags(now_ms, *local_time);

    char clock[8];
    snprintf(clock, sizeof(clock), "%02d:%02d", local_time->tm_hour, local_time->tm_min);
    seg_set_text(s_clock, clock);

    if (s_idle_opa > LV_OPA_MIN) {
        if (settings.clock_face != CLOCK_FACE_FLOWER_POWER) {
            lv_arc_set_value(s_seconds_arc, (int16_t)(local_time->tm_sec * 10 + millis_in_second / 100));
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
