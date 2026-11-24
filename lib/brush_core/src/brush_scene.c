#include "brush_scene.h"

#include <math.h>
#include <string.h>

#define PI_F 3.14159265f
#define SCREEN_CENTER (BRUSH_SCREEN_SIZE / 2.0f)

/* Transition timings (ms); mirrored in preview/sim.js */
#define T_MOVE 450
#define T_APPEAR 520
#define T_APPEAR_DELAY_FROM_IDLE 200
#define T_DISAPPEAR 280
#define T_CLOCK 420
#define T_IDLE_OUT 220
#define T_IDLE_IN 400
#define T_IDLE_IN_DELAY 250
#define T_ADD_IN 300
#define T_ADD_IN_DELAY 380

float brush_ease(brush_ease_t ease, float t)
{
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    float u;
    switch (ease) {
    case EASE_OUT_CUBIC:
        u = 1.0f - t;
        return 1.0f - u * u * u;
    case EASE_IN_CUBIC:
        return t * t * t;
    case EASE_IN_OUT_CUBIC:
        if (t < 0.5f) {
            return 4.0f * t * t * t;
        }
        u = -2.0f * t + 2.0f;
        return 1.0f - u * u * u / 2.0f;
    case EASE_OUT_BACK: {
        const float c1 = 1.2f, c3 = c1 + 1.0f;
        u = t - 1.0f;
        return 1.0f + c3 * u * u * u + c1 * u * u;
    }
    case EASE_LINEAR:
    default:
        return t;
    }
}

void brush_tween_snap(brush_tween_t *t, const brush_geom_t *g, uint32_t now_ms)
{
    t->from = *g;
    t->to = *g;
    t->start_ms = now_ms;
    t->delay_ms = 0;
    t->dur_ms = 0;
    t->ease = EASE_LINEAR;
}

void brush_tween_to(brush_tween_t *t, const brush_geom_t *to, uint32_t now_ms, uint16_t delay_ms,
                    uint16_t dur_ms, brush_ease_t ease)
{
    brush_geom_t cur;
    brush_tween_value(t, now_ms, &cur);
    t->from = cur;
    t->to = *to;
    t->start_ms = now_ms;
    t->delay_ms = delay_ms;
    t->dur_ms = dur_ms;
    t->ease = (uint8_t)ease;
}

static float lerp(float a, float b, float k) { return a + (b - a) * k; }

void brush_tween_value(const brush_tween_t *t, uint32_t now_ms, brush_geom_t *out)
{
    int32_t dt = (int32_t)(now_ms - t->start_ms) - t->delay_ms;
    if (t->dur_ms == 0 || dt >= t->dur_ms) {
        *out = t->to;
        return;
    }
    if (dt <= 0) {
        *out = t->from;
        return;
    }
    float k = brush_ease((brush_ease_t)t->ease, (float)dt / (float)t->dur_ms);
    out->cx = lerp(t->from.cx, t->to.cx, k);
    out->cy = lerp(t->from.cy, t->to.cy, k);
    out->r = lerp(t->from.r, t->to.r, k);
    out->opa = lerp(t->from.opa, t->to.opa, k);
}

bool brush_tween_running(const brush_tween_t *t, uint32_t now_ms)
{
    return t->dur_ms > 0 && (int32_t)(now_ms - t->start_ms) < (int32_t)(t->delay_ms + t->dur_ms);
}

static brush_geom_t geom(float cx, float cy, float r, float opa)
{
    brush_geom_t g = {cx, cy, r, opa};
    return g;
}

static brush_geom_t slot_geom(brush_circle_t c, float opa) { return geom(c.cx, c.cy, c.r, opa); }

static void slots_for(const brush_core_t *core, int8_t slot_of[BRUSH_MAX_TIMERS])
{
    for (int c = 0; c < BRUSH_MAX_TIMERS; c++) {
        slot_of[c] = -1;
    }
    for (int i = 0; i < core->count; i++) {
        slot_of[core->timers[i].color % BRUSH_MAX_TIMERS] = (int8_t)i;
    }
}

void brush_scene_init(brush_scene_t *s, const brush_core_t *core, uint32_t now_ms)
{
    memset(s, 0, sizeof(*s));
    brush_layout_t l;
    brush_layout(core->count, &l);
    s->count = (int8_t)core->count;
    slots_for(core, s->card_slot);

    brush_geom_t g = geom(SCREEN_CENTER, l.clock_cy, l.clock_h, 1.0f);
    brush_tween_snap(&s->clock, &g, now_ms);
    g = geom(0, 0, 0, core->count == 0 ? 1.0f : 0.0f);
    brush_tween_snap(&s->idle, &g, now_ms);
    for (int c = 0; c < BRUSH_MAX_TIMERS; c++) {
        int slot = s->card_slot[c];
        g = slot >= 0 ? slot_geom(l.slots[slot], 1.0f) : geom(SCREEN_CENTER, SCREEN_CENTER, 0, 0);
        brush_tween_snap(&s->cards[c], &g, now_ms);
    }
    g = slot_geom(l.add, l.add.r > 0 ? 1.0f : 0.0f);
    brush_tween_snap(&s->add, &g, now_ms);
}

bool brush_scene_sync(brush_scene_t *s, const brush_core_t *core, uint32_t now_ms)
{
    int8_t slot_of[BRUSH_MAX_TIMERS];
    slots_for(core, slot_of);
    const int n = core->count;
    bool changed = n != s->count;
    for (int c = 0; c < BRUSH_MAX_TIMERS && !changed; c++) {
        changed = slot_of[c] != s->card_slot[c];
    }
    if (!changed) {
        return false;
    }

    brush_layout_t old_l, l;
    brush_layout(s->count, &old_l);
    brush_layout(n, &l);
    const bool from_idle = s->count == 0 && n > 0;
    const bool to_idle = s->count > 0 && n == 0;

    for (int c = 0; c < BRUSH_MAX_TIMERS; c++) {
        const int was = s->card_slot[c];
        const int now_slot = slot_of[c];
        brush_tween_t *t = &s->cards[c];
        if (now_slot >= 0) {
            brush_geom_t target = slot_geom(l.slots[now_slot], 1.0f);
            if (was < 0) {
                /* New ring: grows out of the "+" it replaces, or blooms in place from idle. */
                brush_geom_t start = (!from_idle && old_l.add.r > 0)
                                         ? slot_geom(old_l.add, 0.0f)
                                         : geom(target.cx, target.cy, target.r * 0.6f, 0.0f);
                brush_tween_snap(t, &start, now_ms);
                brush_tween_to(t, &target, now_ms, from_idle ? T_APPEAR_DELAY_FROM_IDLE : 0, T_APPEAR,
                               EASE_OUT_BACK);
            } else {
                brush_tween_to(t, &target, now_ms, 0, T_MOVE, EASE_IN_OUT_CUBIC);
            }
        } else if (was >= 0) {
            brush_geom_t cur;
            brush_tween_value(t, now_ms, &cur);
            brush_geom_t end = geom(cur.cx, cur.cy, cur.r * 0.75f, 0.0f);
            brush_tween_to(t, &end, now_ms, 0, T_DISAPPEAR, EASE_IN_CUBIC);
        }
        s->card_slot[c] = (int8_t)now_slot;
    }

    brush_geom_t g = geom(SCREEN_CENTER, l.clock_cy, l.clock_h, 1.0f);
    brush_tween_to(&s->clock, &g, now_ms, to_idle ? 80 : 0, T_CLOCK, EASE_IN_OUT_CUBIC);

    if (from_idle) {
        g = geom(0, 0, 0, 0.0f);
        brush_tween_to(&s->idle, &g, now_ms, 0, T_IDLE_OUT, EASE_OUT_CUBIC);
    } else if (to_idle) {
        g = geom(0, 0, 0, 1.0f);
        brush_tween_to(&s->idle, &g, now_ms, T_IDLE_IN_DELAY, T_IDLE_IN, EASE_OUT_CUBIC);
    }

    /* The "+" hides immediately (a new ring usually emerges from it) and fades in at its new spot. */
    g = slot_geom(l.add, 0.0f);
    brush_tween_snap(&s->add, &g, now_ms);
    if (l.add.r > 0) {
        g.opa = 1.0f;
        brush_tween_to(&s->add, &g, now_ms, T_ADD_IN_DELAY, T_ADD_IN, EASE_OUT_CUBIC);
    }

    s->count = (int8_t)n;
    return true;
}

void brush_scene_pulse(brush_scene_t *s, uint8_t color, uint16_t amp_permille, uint16_t dur_ms,
                       uint32_t now_ms)
{
    color %= BRUSH_MAX_TIMERS;
    s->pulse_start[color] = now_ms;
    s->pulse_dur[color] = dur_ms;
    s->pulse_amp[color] = amp_permille;
}

static bool pulse_running(const brush_scene_t *s, int c, uint32_t now_ms)
{
    return s->pulse_dur[c] > 0 && (now_ms - s->pulse_start[c]) < s->pulse_dur[c];
}

bool brush_scene_animating(const brush_scene_t *s, uint32_t now_ms)
{
    if (brush_tween_running(&s->clock, now_ms) || brush_tween_running(&s->idle, now_ms) ||
        brush_tween_running(&s->add, now_ms)) {
        return true;
    }
    for (int c = 0; c < BRUSH_MAX_TIMERS; c++) {
        if (brush_tween_running(&s->cards[c], now_ms) || pulse_running(s, c, now_ms)) {
            return true;
        }
    }
    return false;
}

void brush_scene_card(const brush_scene_t *s, uint8_t color, uint32_t now_ms, brush_geom_t *out)
{
    color %= BRUSH_MAX_TIMERS;
    brush_tween_value(&s->cards[color], now_ms, out);
    if (pulse_running(s, color, now_ms)) {
        float k = (float)(now_ms - s->pulse_start[color]) / (float)s->pulse_dur[color];
        out->r *= 1.0f + (float)s->pulse_amp[color] / 1000.0f * sinf(PI_F * k);
    }
}
