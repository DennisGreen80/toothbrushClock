// Scalable toothbrush icon drawn from rounded rectangles (handle, neck, head, bristle tufts).
// Geometry is mirrored in preview/sim.js.
#pragma once

#include <lvgl.h>

// Height is width * 40 / 100.
lv_obj_t *brush_icon_create(lv_obj_t *parent);
void brush_icon_set_width(lv_obj_t *icon, lv_coord_t width);
void brush_icon_set_colors(lv_obj_t *icon, lv_color_t body, lv_color_t bristles, lv_opa_t opa);
