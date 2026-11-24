#include <math.h>
#include <unity.h>

#include "brush_core.h"
#include "brush_i18n.h"
#include "brush_oralb.h"
#include "brush_scene.h"

static brush_core_t core;
static brush_event_t ev[8];

static void init_default(void)
{
    brush_cfg_t cfg = {120000, 30000, 60000, 6000};
    brush_init(&core, &cfg);
}

void setUp(void) { init_default(); }
void tearDown(void) {}

static void test_add_until_full_and_colors_are_reused(void)
{
    for (int i = 0; i < BRUSH_MAX_TIMERS; i++) {
        TEST_ASSERT_EQUAL_INT(i, brush_add(&core, 0));
        TEST_ASSERT_EQUAL_UINT8(i, core.timers[i].color);
    }
    TEST_ASSERT_EQUAL_INT(-1, brush_add(&core, 0));

    TEST_ASSERT_TRUE(brush_remove(&core, 1));
    TEST_ASSERT_EQUAL_UINT8(3, core.count);
    TEST_ASSERT_EQUAL_UINT8(2, core.timers[1].color); /* packed, colors stay with timers */
    TEST_ASSERT_EQUAL_INT(3, brush_add(&core, 0));
    TEST_ASSERT_EQUAL_UINT8(1, core.timers[3].color); /* freed color is reused */
}

static void test_marks_fire_once_at_each_interval_then_done(void)
{
    brush_add(&core, 1000);
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 1000 + 29999, ev, 8));

    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 1000 + 30000, ev, 8));
    TEST_ASSERT_EQUAL_INT(BEV_MARK, ev[0].type);
    TEST_ASSERT_EQUAL_UINT8(1, ev[0].mark);
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 1000 + 30500, ev, 8));

    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 1000 + 60000, ev, 8));
    TEST_ASSERT_EQUAL_UINT8(2, ev[0].mark);
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 1000 + 90000, ev, 8));
    TEST_ASSERT_EQUAL_UINT8(3, ev[0].mark);

    /* No 4th short mark at 120 s: the end is signalled as DONE instead. */
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 1000 + 120000, ev, 8));
    TEST_ASSERT_EQUAL_INT(BEV_DONE, ev[0].type);
    TEST_ASSERT_EQUAL_INT(BT_DONE, core.timers[0].state);
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 1000 + 125000, ev, 8));
}

static void test_skipped_marks_only_signal_latest(void)
{
    brush_add(&core, 0);
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 65000, ev, 8));
    TEST_ASSERT_EQUAL_UINT8(2, ev[0].mark);
}

static void test_pause_excludes_time(void)
{
    brush_add(&core, 0);
    brush_tick(&core, 10000, ev, 8);
    TEST_ASSERT_EQUAL_INT(BT_PAUSED, brush_toggle_pause(&core, 0, 10000));
    TEST_ASSERT_EQUAL_UINT32(10000, brush_elapsed_ms(&core, 0, 50000));
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 50000, ev, 8));
    TEST_ASSERT_EQUAL_INT(BT_RUNNING, brush_toggle_pause(&core, 0, 50000));
    TEST_ASSERT_EQUAL_UINT32(15000, brush_elapsed_ms(&core, 0, 55000));
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 69999, ev, 8));
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 70000, ev, 8));
}

static void test_done_timer_is_auto_cleared(void)
{
    brush_add(&core, 0);
    brush_add(&core, 100000);
    brush_tick(&core, 120000, ev, 8);
    brush_tick(&core, 179999, ev, 8); /* absorbs timer 1's 60 s mark */
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 180000, ev, 8));
    TEST_ASSERT_EQUAL_INT(BEV_CLEARED, ev[0].type);
    TEST_ASSERT_EQUAL_UINT8(0, ev[0].index);
    TEST_ASSERT_EQUAL_UINT8(1, core.count);
    TEST_ASSERT_EQUAL_UINT32(100000, core.timers[0].start_ms);
}

static void test_auto_clear_disabled(void)
{
    brush_cfg_t cfg = {120000, 30000, 0};
    brush_init(&core, &cfg);
    brush_add(&core, 0);
    brush_tick(&core, 120000, ev, 8);
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 999999, ev, 8));
    TEST_ASSERT_EQUAL_UINT8(1, core.count);
}

static void test_millis_wraparound(void)
{
    uint32_t start = 0xFFFFFFFFu - 5000u;
    brush_add(&core, start);
    TEST_ASSERT_EQUAL_UINT32(30000, brush_elapsed_ms(&core, 0, start + 30000u));
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, start + 30000u, ev, 8));
    TEST_ASSERT_EQUAL_INT(BEV_MARK, ev[0].type);
}

static void test_first_color_is_used_only_when_no_timers_run(void)
{
    core.first_color = 2;
    TEST_ASSERT_EQUAL_INT(0, brush_add(&core, 0));
    TEST_ASSERT_EQUAL_UINT8(2, core.timers[0].color);
    brush_add(&core, 0);
    TEST_ASSERT_EQUAL_UINT8(0, core.timers[1].color); /* then the lowest free slot */
}

static void test_rating_thresholds(void)
{
    brush_add(&core, 0);
    TEST_ASSERT_EQUAL_UINT8(0, brush_rating(&core, 0, 29999));
    TEST_ASSERT_EQUAL_UINT8(1, brush_rating(&core, 0, 30000));
    TEST_ASSERT_EQUAL_UINT8(1, brush_rating(&core, 0, 59999));
    TEST_ASSERT_EQUAL_UINT8(2, brush_rating(&core, 0, 60000));
    TEST_ASSERT_EQUAL_UINT8(2, brush_rating(&core, 0, 90999));
    TEST_ASSERT_EQUAL_UINT8(3, brush_rating(&core, 0, 91000));
    TEST_ASSERT_EQUAL_UINT8(3, brush_rating(&core, 0, 150000));
}

static void test_finish_freezes_then_clears_after_hold(void)
{
    brush_add(&core, 0);
    brush_add(&core, 0);
    brush_tick(&core, 45000, ev, 8);
    TEST_ASSERT_TRUE(brush_finish(&core, 0, 45000));
    TEST_ASSERT_TRUE(core.timers[0].finishing);
    TEST_ASSERT_EQUAL_UINT32(45000, brush_elapsed_ms(&core, 0, 50000));
    TEST_ASSERT_EQUAL_UINT8(1, brush_rating(&core, 0, 50000));
    TEST_ASSERT_EQUAL_INT(BT_PAUSED, brush_toggle_pause(&core, 0, 50000)); /* taps don't resume it */

    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 50999, ev, 8));
    TEST_ASSERT_EQUAL_UINT8(2, core.count);
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 51000, ev, 8));
    TEST_ASSERT_EQUAL_INT(BEV_CLEARED, ev[0].type);
    TEST_ASSERT_EQUAL_UINT8(0, ev[0].color);
    TEST_ASSERT_EQUAL_UINT8(1, core.count);
    TEST_ASSERT_EQUAL_UINT8(1, core.timers[0].color);
}

static void test_finish_removes_done_timer_or_without_hold(void)
{
    brush_add(&core, 0);
    brush_tick(&core, 120000, ev, 8);
    TEST_ASSERT_FALSE(brush_finish(&core, 0, 120000));
    TEST_ASSERT_EQUAL_UINT8(0, core.count);

    brush_cfg_t cfg = {120000, 30000, 60000, 0};
    brush_init(&core, &cfg);
    brush_add(&core, 0);
    TEST_ASSERT_FALSE(brush_finish(&core, 0, 10000));
    TEST_ASSERT_EQUAL_UINT8(0, core.count);
}

static void test_display_seconds(void)
{
    bool over = true;
    char buf[8];
    brush_add(&core, 0);

    TEST_ASSERT_EQUAL_UINT32(120, brush_display_seconds(&core, 0, 0, false, &over));
    TEST_ASSERT_FALSE(over);
    TEST_ASSERT_EQUAL_UINT32(120, brush_display_seconds(&core, 0, 1, false, NULL));
    TEST_ASSERT_EQUAL_UINT32(1, brush_display_seconds(&core, 0, 119500, false, NULL));
    TEST_ASSERT_EQUAL_UINT32(119, brush_display_seconds(&core, 0, 119500, true, NULL));

    TEST_ASSERT_EQUAL_UINT32(5, brush_display_seconds(&core, 0, 125000, false, &over));
    TEST_ASSERT_TRUE(over);
    TEST_ASSERT_EQUAL_UINT32(125, brush_display_seconds(&core, 0, 125000, true, &over));

    brush_format_mmss(125, buf);
    TEST_ASSERT_EQUAL_STRING("2:05", buf);
    brush_format_mmss(0, buf);
    TEST_ASSERT_EQUAL_STRING("0:00", buf);
    brush_format_mmss(615, buf);
    TEST_ASSERT_EQUAL_STRING("10:15", buf);
}

static void test_quadrants(void)
{
    brush_add(&core, 0);
    TEST_ASSERT_EQUAL_INT(0, brush_current_quadrant(&core, 0, 29999));
    TEST_ASSERT_EQUAL_INT(1, brush_current_quadrant(&core, 0, 30000));
    TEST_ASSERT_EQUAL_INT(3, brush_current_quadrant(&core, 0, 500000));
    TEST_ASSERT_EQUAL_UINT16(1000, brush_quadrant_permille(&core, 0, 0, 45000));
    TEST_ASSERT_EQUAL_UINT16(500, brush_quadrant_permille(&core, 0, 1, 45000));
    TEST_ASSERT_EQUAL_UINT16(0, brush_quadrant_permille(&core, 0, 2, 45000));
    TEST_ASSERT_EQUAL_UINT16(375, brush_progress_permille(&core, 0, 45000));
}

static bool inside_screen(brush_circle_t c)
{
    double half = BRUSH_SCREEN_SIZE / 2.0;
    double d = sqrt((c.cx - half) * (c.cx - half) + (c.cy - half) * (c.cy - half));
    return d + c.r <= half;
}

static bool overlap(brush_circle_t a, brush_circle_t b)
{
    double d = sqrt((double)(a.cx - b.cx) * (a.cx - b.cx) + (double)(a.cy - b.cy) * (a.cy - b.cy));
    return d < a.r + b.r;
}

static void test_layout_fits_round_screen_without_overlap(void)
{
    for (int n = 0; n <= BRUSH_MAX_TIMERS; n++) {
        brush_layout_t l;
        brush_layout(n, &l);
        TEST_ASSERT_EQUAL_UINT8(n, l.n_slots);
        TEST_ASSERT_TRUE(l.clock_h > 0);
        for (int i = 0; i < n; i++) {
            TEST_ASSERT_TRUE(inside_screen(l.slots[i]));
            for (int j = i + 1; j < n; j++) {
                TEST_ASSERT_FALSE(overlap(l.slots[i], l.slots[j]));
            }
            if (l.add.r > 0) {
                TEST_ASSERT_FALSE(overlap(l.slots[i], l.add));
            }
            /* Clock digits sit above every timer ring. */
            TEST_ASSERT_TRUE(l.clock_cy + l.clock_h / 2 < l.slots[i].cy - l.slots[i].r);
        }
        if (l.add.r > 0) {
            TEST_ASSERT_TRUE(inside_screen(l.add));
        }
        TEST_ASSERT_EQUAL(n > 0 && n < BRUSH_MAX_TIMERS, l.add.r > 0);
    }
}

static void assert_geom_at_slot(brush_geom_t g, brush_circle_t c)
{
    TEST_ASSERT_FLOAT_WITHIN(0.01f, c.cx, g.cx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, c.cy, g.cy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, c.r, g.r);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, g.opa);
}

static void test_ease_endpoints(void)
{
    for (int e = EASE_LINEAR; e <= EASE_OUT_BACK; e++) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, brush_ease((brush_ease_t)e, 0.0f));
        TEST_ASSERT_EQUAL_FLOAT(1.0f, brush_ease((brush_ease_t)e, 1.0f));
    }
    TEST_ASSERT_TRUE(brush_ease(EASE_OUT_BACK, 0.8f) > 1.0f); /* overshoots before settling */
}

static void test_scene_first_timer_shrinks_clock_and_grows_ring(void)
{
    brush_scene_t s;
    brush_layout_t idle, one;
    brush_layout(0, &idle);
    brush_layout(1, &one);
    brush_scene_init(&s, &core, 0);
    TEST_ASSERT_FALSE(brush_scene_animating(&s, 0));

    brush_add(&core, 1000);
    TEST_ASSERT_TRUE(brush_scene_sync(&s, &core, 1000));
    TEST_ASSERT_FALSE(brush_scene_sync(&s, &core, 1010));

    brush_geom_t g;
    brush_tween_value(&s.clock, 1000, &g);
    TEST_ASSERT_EQUAL_FLOAT(idle.clock_h, g.r); /* starts big... */
    brush_tween_value(&s.clock, 1250, &g);
    TEST_ASSERT_TRUE(g.r < idle.clock_h && g.r > one.clock_h); /* ...is mid-way... */
    brush_scene_card(&s, 0, 1000, &g);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g.opa); /* ring not visible yet */

    TEST_ASSERT_TRUE(brush_scene_animating(&s, 1500));
    TEST_ASSERT_FALSE(brush_scene_animating(&s, 3000));
    brush_tween_value(&s.clock, 3000, &g);
    TEST_ASSERT_EQUAL_FLOAT(one.clock_h, g.r); /* ...and ends small */
    brush_tween_value(&s.idle, 3000, &g);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g.opa);
    brush_scene_card(&s, 0, 3000, &g);
    assert_geom_at_slot(g, one.slots[0]);
}

static void test_scene_new_ring_emerges_from_add_button(void)
{
    brush_scene_t s;
    brush_layout_t one, two;
    brush_layout(1, &one);
    brush_layout(2, &two);
    brush_add(&core, 0);
    brush_scene_init(&s, &core, 0);

    brush_add(&core, 100);
    brush_scene_sync(&s, &core, 100);
    brush_geom_t g;
    brush_scene_card(&s, 1, 100, &g);
    TEST_ASSERT_EQUAL_FLOAT(one.add.cx, g.cx);
    TEST_ASSERT_EQUAL_FLOAT(one.add.cy, g.cy);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g.opa);

    brush_scene_card(&s, 0, 5000, &g);
    assert_geom_at_slot(g, two.slots[0]);
    brush_scene_card(&s, 1, 5000, &g);
    assert_geom_at_slot(g, two.slots[1]);
    brush_tween_value(&s.add, 5000, &g);
    assert_geom_at_slot(g, two.add);
}

static void test_scene_removed_ring_fades_and_others_move_up(void)
{
    brush_scene_t s;
    brush_layout_t one;
    brush_layout(1, &one);
    brush_add(&core, 0);
    brush_add(&core, 0);
    brush_scene_init(&s, &core, 0);

    brush_remove(&core, 0); /* color 0 removed, color 1 becomes the only timer */
    brush_scene_sync(&s, &core, 100);
    TEST_ASSERT_EQUAL_INT8(-1, s.card_slot[0]);
    TEST_ASSERT_EQUAL_INT8(0, s.card_slot[1]);

    brush_geom_t g;
    brush_scene_card(&s, 0, 5000, &g);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, g.opa);
    brush_scene_card(&s, 1, 5000, &g);
    assert_geom_at_slot(g, one.slots[0]);
}

static void test_scene_pulse_returns_to_rest(void)
{
    brush_scene_t s;
    brush_layout_t one;
    brush_layout(1, &one);
    brush_add(&core, 0);
    brush_scene_init(&s, &core, 0);

    brush_scene_pulse(&s, 0, 100, 400, 1000);
    brush_geom_t g;
    brush_scene_card(&s, 0, 1200, &g);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, one.slots[0].r * 1.1f, g.r);
    TEST_ASSERT_TRUE(brush_scene_animating(&s, 1300));
    brush_scene_card(&s, 0, 1400, &g);
    assert_geom_at_slot(g, one.slots[0]);
    TEST_ASSERT_FALSE(brush_scene_animating(&s, 1400));
}

static void test_date_format_per_language(void)
{
    char buf[40];
    brush_format_date(BRUSH_LANG_EN, 1, 23, 10, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("Monday, 23 Nov", buf);
    brush_format_date(BRUSH_LANG_DA, 6, 5, 4, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("L\xC3\xB8rdag 5. maj", buf); /* "Lørdag 5. maj" */
}

/* ---- Oral-B ------------------------------------------------------------------------------ */

static const uint8_t BRUSH_A[6] = {0x78, 0xDB, 0x2F, 0xC2, 0x48, 0xBE};
static const uint8_t BRUSH_B[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

static oralb_adv_t adv(bool running, uint16_t secs, bool high)
{
    oralb_adv_t a = {7, 0x34, running ? ORALB_STATE_RUNNING : 2, running, high, secs, 0};
    return a;
}

static void test_oralb_parse_real_advertisements(void)
{
    oralb_adv_t a;
    /* iO Series 4 while brushing with the power button flag set (from oralb-ble tests) */
    const uint8_t running[] = {0xDC, 0x00, 0x07, 0x34, 0x0C, 0x03, 0x38, 0x00, 0x00, 0x02, 0x01, 0x00, 0x04};
    TEST_ASSERT_TRUE(brush_oralb_parse(running, sizeof(running), &a));
    TEST_ASSERT_EQUAL_UINT8(7, a.protocol);
    TEST_ASSERT_TRUE(a.running);
    TEST_ASSERT_FALSE(a.pressure_high);

    /* iO idle */
    const uint8_t idle[] = {0xDC, 0x00, 0x06, 0x32, 0x6B, 0x02, 0x72, 0x00, 0x00, 0x01, 0x01, 0x00, 0x04};
    TEST_ASSERT_TRUE(brush_oralb_parse(idle, sizeof(idle), &a));
    TEST_ASSERT_FALSE(a.running);

    /* brushing too hard at 1:05 */
    const uint8_t hard[] = {0xDC, 0x00, 0x07, 0x34, 0x0C, 0x03, 0xB2, 0x01, 0x05, 0x02, 0x02, 0x00, 0x04};
    TEST_ASSERT_TRUE(brush_oralb_parse(hard, sizeof(hard), &a));
    TEST_ASSERT_TRUE(a.pressure_high);
    TEST_ASSERT_EQUAL_UINT16(65, a.brush_time_s);

    const uint8_t other_company[] = {0x4C, 0x00, 0x07, 0x34, 0x0C, 0x03, 0xB2, 0x01, 0x05, 0x02, 0x02};
    TEST_ASSERT_FALSE(brush_oralb_parse(other_company, sizeof(other_company), &a));
    TEST_ASSERT_FALSE(brush_oralb_parse(hard, 8, &a));
}

static void test_oralb_start_syncs_to_brush_without_replaying_marks(void)
{
    oralb_tracker_t t;
    brush_oralb_init(&t);
    oralb_adv_t a = adv(true, 70, true);
    TEST_ASSERT_EQUAL_INT(ORALB_STARTED, brush_oralb_on_adv(&t, &core, BRUSH_A, &a, 5000));
    TEST_ASSERT_EQUAL_UINT8(1, core.count);
    TEST_ASSERT_EQUAL_UINT8(BRUSH_SRC_ORALB, core.timers[0].source);
    TEST_ASSERT_TRUE(core.timers[0].pressure_high);
    TEST_ASSERT_EQUAL_UINT32(70000, brush_elapsed_ms(&core, 0, 5000));
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 5000, ev, 8)); /* 0:30 and 1:00 are not re-announced */

    a = adv(true, 71, false);
    TEST_ASSERT_EQUAL_INT(ORALB_NONE, brush_oralb_on_adv(&t, &core, BRUSH_A, &a, 6000));
    TEST_ASSERT_FALSE(core.timers[0].pressure_high);
}

static void test_oralb_pause_resume_and_end(void)
{
    oralb_tracker_t t;
    brush_oralb_init(&t);
    oralb_adv_t on = adv(true, 0, false), off = adv(false, 40, false);
    brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 0);

    TEST_ASSERT_EQUAL_INT(ORALB_PAUSED, brush_oralb_on_adv(&t, &core, BRUSH_A, &off, 40000));
    TEST_ASSERT_TRUE(brush_is_paused(&core, 0));
    TEST_ASSERT_EQUAL_UINT32(40000, brush_elapsed_ms(&core, 0, 60000));

    on = adv(true, 40, false);
    TEST_ASSERT_EQUAL_INT(ORALB_RESUMED, brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 60000));
    TEST_ASSERT_FALSE(brush_is_paused(&core, 0));
    TEST_ASSERT_EQUAL_UINT8(1, core.count); /* same timer, not a new one */

    brush_oralb_on_adv(&t, &core, BRUSH_A, &off, 61000);
    TEST_ASSERT_EQUAL_INT(ORALB_NONE, brush_oralb_tick(&t, &core, 61000 + ORALB_RESUME_WINDOW_MS));
    TEST_ASSERT_EQUAL_INT(ORALB_ENDED, brush_oralb_tick(&t, &core, 61001 + ORALB_RESUME_WINDOW_MS));
    TEST_ASSERT_EQUAL_UINT8(0, core.count);
}

static void test_oralb_silence_pauses_and_finished_timer_freezes(void)
{
    oralb_tracker_t t;
    brush_oralb_init(&t);
    oralb_adv_t on = adv(true, 0, false);
    brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 0);
    brush_tick(&core, 125000, ev, 8); /* done */
    on = adv(true, 125, false);
    brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 125000);

    TEST_ASSERT_EQUAL_INT(ORALB_NONE, brush_oralb_tick(&t, &core, 125000 + ORALB_ADV_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_INT(ORALB_PAUSED, brush_oralb_tick(&t, &core, 125001 + ORALB_ADV_TIMEOUT_MS));
    TEST_ASSERT_EQUAL_INT(BT_DONE, core.timers[0].state);
    TEST_ASSERT_EQUAL_UINT32(125000, brush_elapsed_ms(&core, 0, 200000)); /* overtime frozen */
    TEST_ASSERT_EQUAL_INT(0, brush_tick(&core, 999999, ev, 8));       /* not auto-cleared */
    TEST_ASSERT_EQUAL_UINT8(1, core.count);
}

static void test_oralb_dismissed_timer_is_not_recreated_and_brushes_are_independent(void)
{
    oralb_tracker_t t;
    brush_oralb_init(&t);
    oralb_adv_t on = adv(true, 3, false);
    brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 0);
    brush_remove(&core, 0);       /* long press */
    brush_add(&core, 1000);       /* someone starts a manual timer (reuses the color) */
    TEST_ASSERT_EQUAL_INT(ORALB_NONE, brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 2000));
    TEST_ASSERT_EQUAL_UINT8(1, core.count);
    TEST_ASSERT_EQUAL_UINT8(BRUSH_SRC_MANUAL, core.timers[0].source);

    TEST_ASSERT_EQUAL_INT(ORALB_STARTED, brush_oralb_on_adv(&t, &core, BRUSH_B, &on, 2000));
    TEST_ASSERT_EQUAL_UINT8(2, core.count);
}

static void test_oralb_cancelled_timer_stays_frozen_and_is_not_recreated(void)
{
    oralb_tracker_t t;
    brush_oralb_init(&t);
    oralb_adv_t on = adv(true, 40, false);
    brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 0);
    TEST_ASSERT_TRUE(brush_finish(&core, 0, 0)); /* long press */

    on = adv(true, 43, false);
    TEST_ASSERT_EQUAL_INT(ORALB_NONE, brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 3000));
    TEST_ASSERT_EQUAL_UINT32(40000, brush_elapsed_ms(&core, 0, 3000)); /* not resynced to the brush */
    TEST_ASSERT_EQUAL_INT(1, brush_tick(&core, 6000, ev, 8));
    TEST_ASSERT_EQUAL_UINT8(0, core.count);

    on = adv(true, 46, false);
    TEST_ASSERT_EQUAL_INT(ORALB_NONE, brush_oralb_on_adv(&t, &core, BRUSH_A, &on, 6000));
    TEST_ASSERT_EQUAL_UINT8(0, core.count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_add_until_full_and_colors_are_reused);
    RUN_TEST(test_marks_fire_once_at_each_interval_then_done);
    RUN_TEST(test_skipped_marks_only_signal_latest);
    RUN_TEST(test_pause_excludes_time);
    RUN_TEST(test_done_timer_is_auto_cleared);
    RUN_TEST(test_auto_clear_disabled);
    RUN_TEST(test_millis_wraparound);
    RUN_TEST(test_first_color_is_used_only_when_no_timers_run);
    RUN_TEST(test_rating_thresholds);
    RUN_TEST(test_finish_freezes_then_clears_after_hold);
    RUN_TEST(test_finish_removes_done_timer_or_without_hold);
    RUN_TEST(test_display_seconds);
    RUN_TEST(test_quadrants);
    RUN_TEST(test_layout_fits_round_screen_without_overlap);
    RUN_TEST(test_ease_endpoints);
    RUN_TEST(test_scene_first_timer_shrinks_clock_and_grows_ring);
    RUN_TEST(test_scene_new_ring_emerges_from_add_button);
    RUN_TEST(test_scene_removed_ring_fades_and_others_move_up);
    RUN_TEST(test_scene_pulse_returns_to_rest);
    RUN_TEST(test_date_format_per_language);
    RUN_TEST(test_oralb_parse_real_advertisements);
    RUN_TEST(test_oralb_start_syncs_to_brush_without_replaying_marks);
    RUN_TEST(test_oralb_pause_resume_and_end);
    RUN_TEST(test_oralb_silence_pauses_and_finished_timer_freezes);
    RUN_TEST(test_oralb_dismissed_timer_is_not_recreated_and_brushes_are_independent);
    RUN_TEST(test_oralb_cancelled_timer_stays_frozen_and_is_not_recreated);
    return UNITY_END();
}
