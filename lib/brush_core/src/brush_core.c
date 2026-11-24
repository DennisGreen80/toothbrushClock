#include "brush_core.h"

#include <string.h>

void brush_init(brush_core_t *c, const brush_cfg_t *cfg)
{
    memset(c, 0, sizeof(*c));
    c->cfg = *cfg;
    if (c->cfg.mark_ms == 0) {
        c->cfg.mark_ms = c->cfg.duration_ms / BRUSH_QUADRANTS;
    }
}

static uint8_t free_color(const brush_core_t *c)
{
    for (uint8_t color = 0; color < BRUSH_MAX_TIMERS; color++) {
        bool used = false;
        for (uint8_t i = 0; i < c->count; i++) {
            if (c->timers[i].color == color) {
                used = true;
                break;
            }
        }
        if (!used) {
            return color;
        }
    }
    return 0;
}

int brush_add(brush_core_t *c, uint32_t now_ms)
{
    return brush_add_at(c, now_ms, 0, BRUSH_SRC_MANUAL);
}

static uint8_t marks_total(const brush_core_t *c)
{
    return (uint8_t)((c->cfg.duration_ms - 1) / c->cfg.mark_ms); /* marks strictly before the end */
}

static uint8_t marks_reached(const brush_core_t *c, uint32_t elapsed_ms)
{
    uint32_t reached = elapsed_ms / c->cfg.mark_ms;
    return reached > marks_total(c) ? marks_total(c) : (uint8_t)reached;
}

int brush_add_at(brush_core_t *c, uint32_t now_ms, uint32_t elapsed_ms, brush_source_t source)
{
    if (c->count >= BRUSH_MAX_TIMERS) {
        return -1;
    }
    brush_timer_t *t = &c->timers[c->count];
    memset(t, 0, sizeof(*t));
    t->state = elapsed_ms >= c->cfg.duration_ms ? BT_DONE : BT_RUNNING;
    t->start_ms = now_ms - elapsed_ms;
    t->marks_fired = marks_reached(c, elapsed_ms);
    t->color = free_color(c);
    t->source = (uint8_t)source;
    if (++c->next_id == 0) {
        c->next_id = 1;
    }
    t->id = c->next_id;
    return c->count++;
}

bool brush_set_paused(brush_core_t *c, int index, bool paused, uint32_t now_ms)
{
    if (index < 0 || index >= c->count || paused == brush_is_paused(c, index)) {
        return false;
    }
    brush_timer_t *t = &c->timers[index];
    if (paused) {
        t->pause_start_ms = now_ms;
        if (t->state == BT_RUNNING) {
            t->state = BT_PAUSED;
        } else {
            t->held = true;
        }
    } else {
        t->paused_total_ms += now_ms - t->pause_start_ms;
        if (t->state == BT_PAUSED) {
            t->state = BT_RUNNING;
        }
        t->held = false;
    }
    return true;
}

bool brush_is_paused(const brush_core_t *c, int index)
{
    if (index < 0 || index >= c->count) {
        return false;
    }
    return c->timers[index].state == BT_PAUSED || c->timers[index].held;
}

void brush_set_elapsed(brush_core_t *c, int index, uint32_t elapsed_ms, uint32_t now_ms)
{
    if (index < 0 || index >= c->count) {
        return;
    }
    brush_timer_t *t = &c->timers[index];
    uint32_t current = brush_elapsed_ms(c, index, now_ms);
    t->start_ms += current - elapsed_ms;
    if (t->marks_fired > marks_reached(c, elapsed_ms)) {
        t->marks_fired = marks_reached(c, elapsed_ms);
    }
}

int brush_index_of_color(const brush_core_t *c, uint8_t color)
{
    for (int i = 0; i < c->count; i++) {
        if (c->timers[i].color == color) {
            return i;
        }
    }
    return -1;
}

int brush_index_of_id(const brush_core_t *c, uint16_t id)
{
    for (int i = 0; i < c->count; i++) {
        if (c->timers[i].id == id) {
            return i;
        }
    }
    return -1;
}

bool brush_remove(brush_core_t *c, int index)
{
    if (index < 0 || index >= c->count) {
        return false;
    }
    for (int i = index; i < c->count - 1; i++) {
        c->timers[i] = c->timers[i + 1];
    }
    c->count--;
    return true;
}

brush_state_t brush_toggle_pause(brush_core_t *c, int index, uint32_t now_ms)
{
    if (index < 0 || index >= c->count) {
        return BT_DONE;
    }
    if (c->timers[index].state != BT_DONE && !c->timers[index].finishing) {
        brush_set_paused(c, index, c->timers[index].state == BT_RUNNING, now_ms);
    }
    return c->timers[index].state;
}

bool brush_finish(brush_core_t *c, int index, uint32_t now_ms)
{
    if (index < 0 || index >= c->count) {
        return false;
    }
    brush_timer_t *t = &c->timers[index];
    if (t->state == BT_DONE || c->cfg.finish_hold_ms == 0) {
        brush_remove(c, index);
        return false;
    }
    if (!t->finishing) {
        brush_set_paused(c, index, true, now_ms);
        t->pressure_high = false;
        t->finishing = true;
        t->finish_ms = now_ms;
    }
    return true;
}

uint8_t brush_rating(const brush_core_t *c, int index, uint32_t now_ms)
{
    const uint32_t seconds = brush_elapsed_ms(c, index, now_ms) / 1000u;
    const uint32_t quarter = c->cfg.duration_ms / 1000u / BRUSH_QUADRANTS;
    if (seconds < quarter) {
        return 0;
    }
    if (seconds < 2 * quarter) {
        return 1;
    }
    return seconds <= 3 * quarter ? 2 : 3;
}

uint32_t brush_elapsed_ms(const brush_core_t *c, int index, uint32_t now_ms)
{
    if (index < 0 || index >= c->count) {
        return 0;
    }
    const brush_timer_t *t = &c->timers[index];
    uint32_t end = brush_is_paused(c, index) ? t->pause_start_ms : now_ms;
    /* Unsigned arithmetic keeps this correct across millis() wrap-around. */
    return (end - t->start_ms) - t->paused_total_ms;
}

int brush_tick(brush_core_t *c, uint32_t now_ms, brush_event_t *events, int max_events)
{
    int n = 0;
    const uint32_t duration = c->cfg.duration_ms;

    for (int i = 0; i < c->count; i++) {
        brush_timer_t *t = &c->timers[i];
        if (t->state != BT_RUNNING) {
            continue;
        }
        uint32_t elapsed = brush_elapsed_ms(c, i, now_ms);

        if (elapsed >= duration) {
            t->state = BT_DONE;
            t->marks_fired = marks_total(c);
            if (n < max_events) {
                events[n++] = (brush_event_t){BEV_DONE, (uint8_t)i, 0, t->color};
            }
            continue;
        }

        uint8_t reached = marks_reached(c, elapsed);
        if (reached > t->marks_fired) {
            /* If several marks were skipped (e.g. a stall) only signal the latest one. */
            t->marks_fired = reached;
            if (n < max_events) {
                events[n++] = (brush_event_t){BEV_MARK, (uint8_t)i, reached, t->color};
            }
        }
    }

    for (int i = c->count - 1; i >= 0; i--) {
        if (c->timers[i].finishing && now_ms - c->timers[i].finish_ms >= c->cfg.finish_hold_ms) {
            uint8_t color = c->timers[i].color;
            brush_remove(c, i);
            if (n < max_events) {
                events[n++] = (brush_event_t){BEV_CLEARED, (uint8_t)i, 0, color};
            }
        }
    }

    if (c->cfg.auto_clear_ms > 0) {
        for (int i = c->count - 1; i >= 0; i--) {
            /* Toothbrush timers end when the brush is switched off (see brush_oralb). */
            if (c->timers[i].state != BT_DONE || c->timers[i].source != BRUSH_SRC_MANUAL) {
                continue;
            }
            if (brush_elapsed_ms(c, i, now_ms) >= duration + c->cfg.auto_clear_ms) {
                uint8_t color = c->timers[i].color;
                brush_remove(c, i);
                if (n < max_events) {
                    events[n++] = (brush_event_t){BEV_CLEARED, (uint8_t)i, 0, color};
                }
            }
        }
    }
    return n;
}

uint16_t brush_progress_permille(const brush_core_t *c, int index, uint32_t now_ms)
{
    uint32_t elapsed = brush_elapsed_ms(c, index, now_ms);
    if (elapsed >= c->cfg.duration_ms) {
        return 1000;
    }
    return (uint16_t)((uint64_t)elapsed * 1000u / c->cfg.duration_ms);
}

uint16_t brush_quadrant_permille(const brush_core_t *c, int index, int quadrant, uint32_t now_ms)
{
    uint32_t elapsed = brush_elapsed_ms(c, index, now_ms);
    uint32_t q_len = c->cfg.duration_ms / BRUSH_QUADRANTS;
    uint32_t q_start = (uint32_t)quadrant * q_len;
    if (elapsed <= q_start) {
        return 0;
    }
    if (elapsed >= q_start + q_len) {
        return 1000;
    }
    return (uint16_t)((uint64_t)(elapsed - q_start) * 1000u / q_len);
}

int brush_current_quadrant(const brush_core_t *c, int index, uint32_t now_ms)
{
    uint32_t q_len = c->cfg.duration_ms / BRUSH_QUADRANTS;
    uint32_t q = brush_elapsed_ms(c, index, now_ms) / q_len;
    return q >= BRUSH_QUADRANTS ? BRUSH_QUADRANTS - 1 : (int)q;
}

uint32_t brush_display_seconds(const brush_core_t *c, int index, uint32_t now_ms,
                               bool count_up, bool *overtime)
{
    uint32_t elapsed = brush_elapsed_ms(c, index, now_ms);
    uint32_t duration = c->cfg.duration_ms;
    bool over = elapsed >= duration;
    if (overtime) {
        *overtime = over;
    }
    if (count_up) {
        return elapsed / 1000u;
    }
    if (over) {
        return (elapsed - duration) / 1000u;
    }
    return (duration - elapsed + 999u) / 1000u;
}

void brush_format_mmss(uint32_t seconds, char *out)
{
    uint32_t m = seconds / 60u;
    uint32_t s = seconds % 60u;
    if (m > 99u) {
        m = 99u;
        s = 59u;
    }
    int p = 0;
    if (m >= 10u) {
        out[p++] = (char)('0' + m / 10u);
    }
    out[p++] = (char)('0' + m % 10u);
    out[p++] = ':';
    out[p++] = (char)('0' + s / 10u);
    out[p++] = (char)('0' + s % 10u);
    out[p] = '\0';
}

static brush_circle_t circle(int cx, int cy, int r)
{
    brush_circle_t b = {(int16_t)cx, (int16_t)cy, (int16_t)r};
    return b;
}

void brush_layout(int timer_count, brush_layout_t *out)
{
    memset(out, 0, sizeof(*out));
    if (timer_count < 0) {
        timer_count = 0;
    }
    if (timer_count > BRUSH_MAX_TIMERS) {
        timer_count = BRUSH_MAX_TIMERS;
    }
    out->n_slots = (uint8_t)timer_count;

    switch (timer_count) {
    case 0:
        out->clock_cy = 212;
        out->clock_h = 110;
        break;
    case 1:
        out->slots[0] = circle(240, 248, 160);
        out->add = circle(240, 440, 26);
        out->clock_cy = 52;
        out->clock_h = 30;
        break;
    case 2:
        out->slots[0] = circle(128, 240, 102);
        out->slots[1] = circle(352, 240, 102);
        out->add = circle(240, 392, 30);
        out->clock_cy = 92;
        out->clock_h = 36;
        break;
    default: {
        static const int16_t grid[BRUSH_MAX_TIMERS][2] = {
            {152, 182}, {328, 182}, {152, 352}, {328, 352}};
        for (int i = 0; i < timer_count; i++) {
            out->slots[i] = circle(grid[i][0], grid[i][1], 80);
        }
        if (timer_count < BRUSH_MAX_TIMERS) {
            out->add = circle(grid[timer_count][0], grid[timer_count][1], 80);
            out->add_is_ghost = true;
        }
        out->clock_cy = 60;
        out->clock_h = 28;
        break;
    }
    }
}
