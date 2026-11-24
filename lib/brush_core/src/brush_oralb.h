/* Oral-B toothbrush support from passive BLE advertisements.
 *
 * Oral-B brushes broadcast their state in manufacturer-specific data (company 0x00DC):
 *   [0] protocol version  [1] model  [2] ?  [3] state (3 = brushing)
 *   [4] status bit field: bit 7 = pressure too high, bit 2/3 = (power) button pressed
 *   [5] minutes  [6] seconds of the brushing session  [7] mode  [8] sector  ([9] [10] on newer)
 * Format as decoded by Home Assistant's oralb-ble (MIT). The pressure *level* (low/optimal)
 * is not in the advertisement; only "too high" is.
 *
 * brush_oralb_on_adv()/brush_oralb_tick() turn that stream into timers in a brush_core_t. */
#ifndef BRUSH_ORALB_H
#define BRUSH_ORALB_H

#include <stddef.h>

#include "brush_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORALB_COMPANY_ID 0x00DC
#define ORALB_STATE_RUNNING 3

/* Brush is considered switched off if no advertisement arrives for this long. */
#define ORALB_ADV_TIMEOUT_MS 4000
/* A paused brush timer resumes if brushing restarts within this window, otherwise it ends. */
#define ORALB_RESUME_WINDOW_MS 30000

typedef struct {
    uint8_t protocol;
    uint8_t model;
    uint8_t state;
    bool running;
    bool pressure_high;
    uint16_t brush_time_s;
    uint8_t mode;
} oralb_adv_t;

/* Parses BLE manufacturer data *including* the two little-endian company id bytes. */
bool brush_oralb_parse(const uint8_t *mfg, size_t len, oralb_adv_t *out);

typedef enum {
    ORALB_NONE = 0,
    ORALB_STARTED, /* a new brush timer was created */
    ORALB_RESUMED,
    ORALB_PAUSED,
    ORALB_ENDED,   /* the brush stayed off; its timer was removed */
    ORALB_FULL,    /* brushing, but all timer slots are in use */
} oralb_action_t;

typedef struct {
    bool active;
    uint8_t addr[6];
    uint16_t timer_id; /* brush_timer_t.id of the linked timer */
    bool dismissed;   /* user removed the timer: ignore this session */
    bool stopped;     /* brush is off; timer paused */
    uint32_t last_seen_ms;
    uint32_t stopped_ms;
} oralb_link_t;

typedef struct {
    oralb_link_t links[BRUSH_MAX_TIMERS];
} oralb_tracker_t;

void brush_oralb_init(oralb_tracker_t *t);
/* Feed one advertisement from brush `addr`. */
oralb_action_t brush_oralb_on_adv(oralb_tracker_t *t, brush_core_t *core, const uint8_t addr[6],
                                  const oralb_adv_t *adv, uint32_t now_ms);
/* Call regularly: handles brushes that went quiet and expired resume windows.
 * Returns the most significant action taken. */
oralb_action_t brush_oralb_tick(oralb_tracker_t *t, brush_core_t *core, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BRUSH_ORALB_H */
