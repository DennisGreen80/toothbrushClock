/* Platform-independent toothbrush timer logic and screen layout.
 * No Arduino/LVGL dependencies so it can be unit tested on the host. */
#ifndef BRUSH_CORE_H
#define BRUSH_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BRUSH_MAX_TIMERS 4
#define BRUSH_QUADRANTS 4

typedef enum {
    BT_RUNNING = 0,
    BT_PAUSED,
    BT_DONE, /* reached the target; keeps counting overtime until cleared */
} brush_state_t;

typedef enum {
    BRUSH_SRC_MANUAL = 0, /* started by tapping the screen */
    BRUSH_SRC_ORALB,      /* started and driven by an Oral-B toothbrush over BLE */
} brush_source_t;

typedef struct {
    brush_state_t state;
    uint32_t start_ms;
    uint32_t paused_total_ms;
    uint32_t pause_start_ms;
    uint8_t marks_fired; /* number of interval marks already signalled */
    uint8_t color;       /* stable color slot 0..BRUSH_MAX_TIMERS-1 */
    uint8_t source;      /* brush_source_t */
    bool held;           /* a DONE timer whose time is frozen (brush switched off) */
    bool pressure_high;  /* Oral-B: brushing too hard right now */
    uint16_t id;         /* unique per timer, never reused while running */
} brush_timer_t;

typedef struct {
    uint32_t duration_ms;   /* target brushing time, e.g. 120000 */
    uint32_t mark_ms;       /* feedback interval, e.g. 30000 */
    uint32_t auto_clear_ms; /* remove a finished manual timer after this much overtime, 0 = never */
} brush_cfg_t;

typedef struct {
    brush_cfg_t cfg;
    brush_timer_t timers[BRUSH_MAX_TIMERS];
    uint8_t count; /* active timers are always packed at [0, count) */
    uint16_t next_id;
} brush_core_t;

typedef enum {
    BEV_MARK = 1, /* an interval mark (30 s, 60 s, ...) was passed; `mark` is 1-based */
    BEV_DONE,     /* target duration reached */
    BEV_CLEARED,  /* timer auto-removed after overtime; indices above `index` shifted down */
} brush_event_type_t;

typedef struct {
    brush_event_type_t type;
    uint8_t index;
    uint8_t mark;
    uint8_t color; /* stable identity of the timer, valid even after indices shifted */
} brush_event_t;

void brush_init(brush_core_t *c, const brush_cfg_t *cfg);

/* Starts a new timer. Returns its index, or -1 if all slots are in use. */
int brush_add(brush_core_t *c, uint32_t now_ms);
/* Starts a timer that has already been running for `elapsed_ms` (e.g. synced to a toothbrush).
 * Marks that already passed are not signalled again. */
int brush_add_at(brush_core_t *c, uint32_t now_ms, uint32_t elapsed_ms, brush_source_t source);
bool brush_remove(brush_core_t *c, int index);
/* Freezes or unfreezes a timer's time. Works for running/paused and finished timers.
 * Returns true if anything changed. */
bool brush_set_paused(brush_core_t *c, int index, bool paused, uint32_t now_ms);
bool brush_is_paused(const brush_core_t *c, int index);
/* Moves a running timer so its elapsed time becomes `elapsed_ms`. */
void brush_set_elapsed(brush_core_t *c, int index, uint32_t elapsed_ms, uint32_t now_ms);
int brush_index_of_color(const brush_core_t *c, uint8_t color);
int brush_index_of_id(const brush_core_t *c, uint16_t id);
/* Pauses a running timer or resumes a paused one. Returns the new state. */
brush_state_t brush_toggle_pause(brush_core_t *c, int index, uint32_t now_ms);

/* Advances all timers. Writes up to `max_events` events; returns how many were written. */
int brush_tick(brush_core_t *c, uint32_t now_ms, brush_event_t *events, int max_events);

uint32_t brush_elapsed_ms(const brush_core_t *c, int index, uint32_t now_ms);
/* 0..1000 progress towards the target. */
uint16_t brush_progress_permille(const brush_core_t *c, int index, uint32_t now_ms);
/* Fill of a single quadrant (0..1000); quadrant q covers [q*mark, (q+1)*mark). */
uint16_t brush_quadrant_permille(const brush_core_t *c, int index, int quadrant, uint32_t now_ms);
/* Index of the quadrant currently being brushed (0..BRUSH_QUADRANTS-1). */
int brush_current_quadrant(const brush_core_t *c, int index, uint32_t now_ms);

/* Seconds to show on the display.
 * Countdown: remaining time (rounded up so it starts at the full duration), then the
 *            overtime since the target counting up from 0:00.
 * Count up:  total elapsed time, continuing past the target.
 * `overtime` (optional) is set when the target has been reached. */
uint32_t brush_display_seconds(const brush_core_t *c, int index, uint32_t now_ms,
                               bool count_up, bool *overtime);
/* Formats seconds as "M:SS" (or "MM:SS"). `out` must hold at least 8 chars. */
void brush_format_mmss(uint32_t seconds, char *out);

/* ---- Layout for a round 480x480 screen ---------------------------------------------- */

#define BRUSH_SCREEN_SIZE 480

typedef struct {
    int16_t cx, cy, r;
} brush_circle_t;

typedef struct {
    uint8_t n_slots;
    brush_circle_t slots[BRUSH_MAX_TIMERS];
    brush_circle_t add; /* "+" button; r == 0 means no button */
    bool add_is_ghost;  /* true: drawn as an empty ring in the free grid slot */
    int16_t clock_cy;   /* vertical center of the clock digits */
    int16_t clock_h;    /* digit height of the clock */
} brush_layout_t;

void brush_layout(int timer_count, brush_layout_t *out);

#ifdef __cplusplus
}
#endif

#endif /* BRUSH_CORE_H */
