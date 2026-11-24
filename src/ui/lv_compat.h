// Small helpers shared by the LVGL 9 UI code.
#pragma once

#include <lvgl.h>

// LVGL 9 replaced lv_coord_t with plain int32_t; a matching typedef is harmless if it still exists.
typedef int32_t lv_coord_t;

inline lv_area_t obj_coords(const lv_obj_t *obj)
{
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    return a;
}

inline bool same_color(lv_color_t a, lv_color_t b) { return lv_color_eq(a, b); }
