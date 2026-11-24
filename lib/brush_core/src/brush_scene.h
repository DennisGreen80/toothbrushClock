/* Animated scene state for the timer UI.
 *
 * Tracks where every element (big clock, idle decorations, timer rings, "+" button)
 * is on screen and tweens it to the layout from brush_layout() whenever timers are
 * added or removed. Timer rings are keyed by the timer's color slot so a ring keeps
 * its identity while other timers come and go. Pure C, no LVGL, so it is host-testable
 * and mirrored 1:1 by preview/sim.js. */
#ifndef BRUSH_SCENE_H
#define BRUSH_SCENE_H

#include "brush_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float cx, cy; /* center */
    float r;      /* radius; for the clock: digit height */
    float opa;    /* 0..1 (may overshoot slightly with EASE_OUT_BACK) */
} brush_geom_t;

typedef enum {
    EASE_LINEAR = 0,
    EASE_OUT_CUBIC,
    EASE_IN_CUBIC,
    EASE_IN_OUT_CUBIC,
    EASE_OUT_BACK, /* slight overshoot, gives a "springy" settle */
} brush_ease_t;

typedef struct {
    brush_geom_t from, to;
    uint32_t start_ms;
    uint16_t delay_ms, dur_ms;
    uint8_t ease;
} brush_tween_t;

float brush_ease(brush_ease_t ease, float t);
void brush_tween_snap(brush_tween_t *t, const brush_geom_t *g, uint32_t now_ms);
/* Starts a new tween from the current (possibly mid-animation) value. */
void brush_tween_to(brush_tween_t *t, const brush_geom_t *to, uint32_t now_ms, uint16_t delay_ms,
                    uint16_t dur_ms, brush_ease_t ease);
void brush_tween_value(const brush_tween_t *t, uint32_t now_ms, brush_geom_t *out);
bool brush_tween_running(const brush_tween_t *t, uint32_t now_ms);

typedef struct {
    brush_tween_t clock; /* cy + digit height (r) */
    brush_tween_t idle;  /* opacity of date, hint and seconds ring */
    brush_tween_t cards[BRUSH_MAX_TIMERS]; /* indexed by timer color slot */
    brush_tween_t add;
    int8_t card_slot[BRUSH_MAX_TIMERS]; /* layout slot per color, -1 = no timer */
    int8_t count;                        /* timer count the scene is laid out for */
    uint32_t pulse_start[BRUSH_MAX_TIMERS];
    uint16_t pulse_dur[BRUSH_MAX_TIMERS];
    uint16_t pulse_amp[BRUSH_MAX_TIMERS]; /* permille of extra radius at the peak */
} brush_scene_t;

/* Lays the scene out for the current timers without animating. */
void brush_scene_init(brush_scene_t *s, const brush_core_t *core, uint32_t now_ms);
/* Starts transitions if timers were added/removed. Returns true if the layout changed. */
bool brush_scene_sync(brush_scene_t *s, const brush_core_t *core, uint32_t now_ms);
/* Briefly "breathes" a ring, e.g. on a 30 s mark or when done. */
void brush_scene_pulse(brush_scene_t *s, uint8_t color, uint16_t amp_permille, uint16_t dur_ms,
                       uint32_t now_ms);
bool brush_scene_animating(const brush_scene_t *s, uint32_t now_ms);
/* Current ring geometry for a color slot, including any pulse. */
void brush_scene_card(const brush_scene_t *s, uint8_t color, uint32_t now_ms, brush_geom_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BRUSH_SCENE_H */
