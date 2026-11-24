/* LVGL 8.3 configuration for the toothbrush timer.
 * Anything not set here falls back to LVGL's defaults (lv_conf_internal.h). */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* With BOARD_HAS_PSRAM, malloc() serves large blocks from PSRAM. */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_DISP_DEF_REFR_PERIOD 20
#define LV_INDEV_DEF_READ_PERIOD 15
#define LV_INDEV_DEF_LONG_PRESS_TIME 700

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/* Montserrat 14/20/28 + LVGL symbols + ÆØÅæøå, generated into src/ui/fonts (see README).
 * The built-in Montserrat fonts are ASCII-only, so they are disabled.
 * Berkshire Swash / Fredoka: clock digits and date for the Christmas faces (optional in settings). */
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(tb_font_14) LV_FONT_DECLARE(tb_font_20) LV_FONT_DECLARE(tb_font_28) \
    LV_FONT_DECLARE(tb_font_berkshire_clock) LV_FONT_DECLARE(tb_font_berkshire_28) \
    LV_FONT_DECLARE(tb_font_fredoka_clock) LV_FONT_DECLARE(tb_font_fredoka_28)
#define LV_FONT_DEFAULT &tb_font_20

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_BUILD_EXAMPLES 0

#endif /* LV_CONF_H */
