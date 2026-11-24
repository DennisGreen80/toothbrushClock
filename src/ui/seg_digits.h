// Scalable "segment" digits drawn with rounded rectangles. Supports 0-9, ':', '-' and ' '.
// Geometry is mirrored in preview/sim.js so the previews match the device.
#pragma once

#include <lvgl.h>

lv_obj_t *seg_create(lv_obj_t *parent, lv_coord_t digit_height);
void seg_set_text(lv_obj_t *seg, const char *text);
void seg_set_color(lv_obj_t *seg, lv_color_t color);
void seg_set_height(lv_obj_t *seg, lv_coord_t digit_height);
void seg_set_opa(lv_obj_t *seg, lv_opa_t opa);
