#include "ui/brush_icon.h"

#include <string.h>

#include "ui/lv_compat.h"

namespace {

struct IconData {
    lv_color_t body;
    lv_color_t bristles;
    lv_opa_t opa;
};

// Shapes in a 100 x 40 unit box: {x1, y1, x2, y2}
const int16_t kBody[][4] = {
    {0, 24, 62, 38},  // handle
    {56, 28, 78, 34}, // neck
    {72, 24, 99, 36}, // head
};
const int16_t kBristles[][4] = {
    {74, 6, 81, 26},
    {83, 4, 90, 26},
    {92, 6, 99, 26},
};

void fill(lv_layer_t *layer, lv_draw_rect_dsc_t *dsc, const lv_area_t &box, lv_coord_t w, const int16_t s[4])
{
    lv_area_t a = {
        (lv_coord_t)(box.x1 + s[0] * w / 100), (lv_coord_t)(box.y1 + s[1] * w / 100),
        (lv_coord_t)(box.x1 + s[2] * w / 100 - 1), (lv_coord_t)(box.y1 + s[3] * w / 100 - 1)};
    lv_draw_rect(layer, dsc, &a);
}

void draw_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    IconData *d = (IconData *)lv_obj_get_user_data(obj);
    if (!d || d->opa <= LV_OPA_MIN) {
        return;
    }
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_area_t coords = obj_coords(obj);
    const lv_coord_t w = lv_obj_get_width(obj);

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.bg_opa = d->opa;
    dsc.bg_color = d->body;
    for (const auto &s : kBody) {
        fill(layer, &dsc, coords, w, s);
    }
    dsc.bg_color = d->bristles;
    for (const auto &s : kBristles) {
        fill(layer, &dsc, coords, w, s);
    }
}

void delete_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_free(lv_obj_get_user_data(obj));
    lv_obj_set_user_data(obj, nullptr);
}

} // namespace

lv_obj_t *brush_icon_create(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_clickable(obj, false);
    lv_obj_set_scrollable(obj, false);
    IconData *d = (IconData *)lv_malloc(sizeof(IconData));
    memset(d, 0, sizeof(*d));
    d->body = lv_color_white();
    d->bristles = lv_color_white();
    d->opa = LV_OPA_COVER;
    lv_obj_set_user_data(obj, d);
    lv_obj_add_event_cb(obj, draw_event_cb, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, delete_event_cb, LV_EVENT_DELETE, nullptr);
    brush_icon_set_width(obj, 40);
    return obj;
}

void brush_icon_set_width(lv_obj_t *icon, lv_coord_t width)
{
    lv_obj_set_size(icon, width, width * 40 / 100);
}

void brush_icon_set_colors(lv_obj_t *icon, lv_color_t body, lv_color_t bristles, lv_opa_t opa)
{
    IconData *d = (IconData *)lv_obj_get_user_data(icon);
    if (lv_color_eq(d->body, body) && lv_color_eq(d->bristles, bristles) &&
        d->opa == opa) {
        return;
    }
    d->body = body;
    d->bristles = bristles;
    d->opa = opa;
    lv_obj_invalidate(icon);
}
