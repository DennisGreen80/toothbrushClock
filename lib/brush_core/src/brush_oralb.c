#include "brush_oralb.h"

#include <string.h>

#define STATUS_PRESSURE_HIGH 0x80
#define STATUS_BUTTON 0x04
#define STATUS_POWER_BUTTON 0x08

/* Re-sync our timer to the brush's own clock if they drift further apart than this. */
#define RESYNC_TOLERANCE_MS 1500

bool brush_oralb_parse(const uint8_t *mfg, size_t len, oralb_adv_t *out)
{
    if (!mfg || len < 2 || (mfg[0] | (mfg[1] << 8)) != ORALB_COMPANY_ID) {
        return false;
    }
    const uint8_t *d = mfg + 2;
    const size_t n = len - 2;
    if (n != 9 && n != 11) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->protocol = d[0];
    out->model = d[1];
    out->state = d[3];
    out->running = d[3] == ORALB_STATE_RUNNING;
    /* Button presses share the byte and take precedence over the pressure flag. */
    out->pressure_high = (d[4] & STATUS_PRESSURE_HIGH) && !(d[4] & (STATUS_BUTTON | STATUS_POWER_BUTTON));
    out->brush_time_s = (uint16_t)(d[5] * 60 + d[6]);
    out->mode = d[7];
    return true;
}

void brush_oralb_init(oralb_tracker_t *t) { memset(t, 0, sizeof(*t)); }

static oralb_link_t *find_link(oralb_tracker_t *t, const uint8_t addr[6])
{
    for (int i = 0; i < BRUSH_MAX_TIMERS; i++) {
        if (t->links[i].active && memcmp(t->links[i].addr, addr, 6) == 0) {
            return &t->links[i];
        }
    }
    return NULL;
}

static oralb_link_t *free_link(oralb_tracker_t *t)
{
    for (int i = 0; i < BRUSH_MAX_TIMERS; i++) {
        if (!t->links[i].active) {
            return &t->links[i];
        }
    }
    return NULL;
}

static oralb_action_t pause_link(oralb_link_t *l, brush_core_t *core, uint32_t now_ms)
{
    l->stopped = true;
    l->stopped_ms = now_ms;
    int idx = brush_index_of_id(core, l->timer_id);
    if (idx < 0 || core->timers[idx].finishing) {
        return ORALB_NONE;
    }
    core->timers[idx].pressure_high = false;
    return brush_set_paused(core, idx, true, now_ms) ? ORALB_PAUSED : ORALB_NONE;
}

oralb_action_t brush_oralb_on_adv(oralb_tracker_t *t, brush_core_t *core, const uint8_t addr[6],
                                  const oralb_adv_t *adv, uint32_t now_ms)
{
    oralb_link_t *l = find_link(t, addr);

    if (!adv->running) {
        if (l && !l->stopped) {
            l->last_seen_ms = now_ms;
            return pause_link(l, core, now_ms);
        }
        return ORALB_NONE;
    }

    const uint32_t brush_ms = (uint32_t)adv->brush_time_s * 1000u;
    if (!l) {
        l = free_link(t);
        if (!l) {
            return ORALB_FULL;
        }
        int idx = brush_add_at(core, now_ms, brush_ms, BRUSH_SRC_ORALB);
        if (idx < 0) {
            return ORALB_FULL;
        }
        memset(l, 0, sizeof(*l));
        l->active = true;
        memcpy(l->addr, addr, 6);
        l->timer_id = core->timers[idx].id;
        l->last_seen_ms = now_ms;
        core->timers[idx].pressure_high = adv->pressure_high;
        return ORALB_STARTED;
    }

    l->last_seen_ms = now_ms;
    const bool was_stopped = l->stopped;
    l->stopped = false;
    if (l->dismissed) {
        return ORALB_NONE;
    }
    int idx = brush_index_of_id(core, l->timer_id);
    if (idx < 0 || core->timers[idx].finishing) {
        l->dismissed = true; /* removed or cancelled by a long press; don't bring it back this session */
        return ORALB_NONE;
    }

    brush_timer_t *tm = &core->timers[idx];
    tm->pressure_high = adv->pressure_high;
    oralb_action_t action = ORALB_NONE;
    if (brush_set_paused(core, idx, false, now_ms) || was_stopped) {
        action = ORALB_RESUMED;
    }
    /* The brush's session clock is authoritative (it also pauses when switched off). */
    uint32_t elapsed = brush_elapsed_ms(core, idx, now_ms);
    if (elapsed + RESYNC_TOLERANCE_MS < brush_ms || elapsed > brush_ms + 1000u + RESYNC_TOLERANCE_MS) {
        brush_set_elapsed(core, idx, brush_ms + 500u, now_ms);
    }
    return action;
}

oralb_action_t brush_oralb_tick(oralb_tracker_t *t, brush_core_t *core, uint32_t now_ms)
{
    oralb_action_t result = ORALB_NONE;
    for (int i = 0; i < BRUSH_MAX_TIMERS; i++) {
        oralb_link_t *l = &t->links[i];
        if (!l->active) {
            continue;
        }
        if (!l->stopped && now_ms - l->last_seen_ms > ORALB_ADV_TIMEOUT_MS) {
            /* Out of range or switched off without us catching the "off" advertisement. */
            if (pause_link(l, core, l->last_seen_ms) == ORALB_PAUSED && result == ORALB_NONE) {
                result = ORALB_PAUSED;
            }
        }
        if (l->stopped && now_ms - l->stopped_ms > ORALB_RESUME_WINDOW_MS) {
            int idx = brush_index_of_id(core, l->timer_id);
            if (!l->dismissed && idx >= 0 && !core->timers[idx].finishing) {
                brush_remove(core, idx);
                result = ORALB_ENDED;
            }
            l->active = false;
        }
    }
    return result;
}
