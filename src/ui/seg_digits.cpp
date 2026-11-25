#include "ui/seg_digits.h"

#include <string.h>

#include "ui/lv_compat.h"

namespace {

constexpr lv_opa_t OFF_SEGMENT_OPA = 18; // faint "unlit" segments

struct SegData {
    char text[12];
    lv_coord_t h;
    lv_color_t color;
    lv_opa_t opa;
};

struct Geometry {
    lv_coord_t h, w, t, gap, spacing;
};

Geometry geometry(lv_coord_t h)
{
    Geometry g;
    g.h = h;
    g.w = (lv_coord_t)(h * 52 / 100);
    g.t = LV_MAX((lv_coord_t)(h * 10 / 100), 2);
    g.gap = LV_MAX((lv_coord_t)(g.t / 4), 1);
    g.spacing = (lv_coord_t)(h * 16 / 100);
    return g;
}

// Bits: a=top, b=top right, c=bottom right, d=bottom, e=bottom left, f=top left, g=middle
uint8_t segments_for(char ch)
{
    static const uint8_t digits[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};
    if (ch >= '0' && ch <= '9') {
        return digits[ch - '0'];
    }
    return ch == '-' ? 0x40 : 0x00;
}

lv_coord_t char_width(const Geometry &g, char ch) { return ch == ':' ? g.t : g.w; }

lv_coord_t text_width(const Geometry &g, const char *text)
{
    lv_coord_t total = 0;
    size_t n = strlen(text);
    for (size_t i = 0; i < n; i++) {
        total += char_width(g, text[i]);
    }
    if (n > 1) {
        total += g.spacing * (lv_coord_t)(n - 1);
    }
    return total;
}

void fill(lv_layer_t *layer, lv_draw_rect_dsc_t *dsc, lv_coord_t x1, lv_coord_t y1, lv_coord_t x2,
          lv_coord_t y2)
{
    lv_area_t a = {x1, y1, (lv_coord_t)(x2 - 1), (lv_coord_t)(y2 - 1)};
    lv_draw_rect(layer, dsc, &a);
}

void draw_digit(lv_layer_t *layer, const Geometry &g, lv_coord_t x, lv_coord_t y, uint8_t on,
                lv_color_t color, lv_opa_t opa)
{
    const lv_coord_t h = g.h, w = g.w, t = g.t, gap = g.gap;
    const lv_coord_t hx1 = x + t / 2 + gap, hx2 = x + w - t / 2 - gap;
    const lv_coord_t mid = y + h / 2;
    struct Seg {
        lv_coord_t x1, y1, x2, y2;
    } segs[7] = {
        {hx1, y, hx2, (lv_coord_t)(y + t)},                                           // a
        {(lv_coord_t)(x + w - t), (lv_coord_t)(y + t / 2 + gap), (lv_coord_t)(x + w), (lv_coord_t)(mid - gap)}, // b
        {(lv_coord_t)(x + w - t), (lv_coord_t)(mid + gap), (lv_coord_t)(x + w), (lv_coord_t)(y + h - t / 2 - gap)}, // c
        {hx1, (lv_coord_t)(y + h - t), hx2, (lv_coord_t)(y + h)},                     // d
        {x, (lv_coord_t)(mid + gap), (lv_coord_t)(x + t), (lv_coord_t)(y + h - t / 2 - gap)},                   // e
        {x, (lv_coord_t)(y + t / 2 + gap), (lv_coord_t)(x + t), (lv_coord_t)(mid - gap)},                       // f
        {hx1, (lv_coord_t)(mid - t / 2), hx2, (lv_coord_t)(mid - t / 2 + t)},         // g
    };

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.bg_color = color;
    for (int i = 0; i < 7; i++) {
        dsc.bg_opa = (lv_opa_t)(((on & (1 << i)) ? LV_OPA_COVER : OFF_SEGMENT_OPA) * opa / 255);
        fill(layer, &dsc, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
    }
}

void draw_colon(lv_layer_t *layer, const Geometry &g, lv_coord_t x, lv_coord_t y, lv_color_t color,
                lv_opa_t opa)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.bg_color = color;
    dsc.bg_opa = opa;
    const lv_coord_t centers[2] = {(lv_coord_t)(y + g.h * 32 / 100), (lv_coord_t)(y + g.h * 68 / 100)};
    for (lv_coord_t cy : centers) {
        fill(layer, &dsc, x, (lv_coord_t)(cy - g.t / 2), (lv_coord_t)(x + g.t), (lv_coord_t)(cy - g.t / 2 + g.t));
    }
}

void draw_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    SegData *d = (SegData *)lv_obj_get_user_data(obj);
    if (!d || d->opa <= LV_OPA_MIN) {
        return;
    }
    lv_layer_t *layer = lv_event_get_layer(e);
    const lv_area_t coords = obj_coords(obj);
    const Geometry g = geometry(d->h);
    lv_coord_t x = coords.x1;
    const lv_coord_t y = coords.y1;
    for (const char *p = d->text; *p; p++) {
        if (*p == ':') {
            draw_colon(layer, g, x, y, d->color, d->opa);
        } else {
            draw_digit(layer, g, x, y, segments_for(*p), d->color, d->opa);
        }
        x += char_width(g, *p) + g.spacing;
    }
}

void delete_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_free(lv_obj_get_user_data(obj));
    lv_obj_set_user_data(obj, nullptr);
}

void resize(lv_obj_t *seg)
{
    SegData *d = (SegData *)lv_obj_get_user_data(seg);
    const Geometry g = geometry(d->h);
    lv_obj_set_size(seg, LV_MAX(text_width(g, d->text), 1), d->h);
}

} // namespace

lv_obj_t *seg_create(lv_obj_t *parent, lv_coord_t digit_height)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_clickable(obj, false);
    lv_obj_set_scrollable(obj, false);

    SegData *d = (SegData *)lv_malloc(sizeof(SegData));
    memset(d, 0, sizeof(*d));
    d->h = digit_height;
    d->color = lv_color_white();
    d->opa = LV_OPA_COVER;
    lv_obj_set_user_data(obj, d);

    lv_obj_add_event_cb(obj, draw_event_cb, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(obj, delete_event_cb, LV_EVENT_DELETE, nullptr);
    resize(obj);
    return obj;
}

void seg_set_text(lv_obj_t *seg, const char *text)
{
    SegData *d = (SegData *)lv_obj_get_user_data(seg);
    if (strncmp(d->text, text, sizeof(d->text)) == 0) {
        return;
    }
    strncpy(d->text, text, sizeof(d->text) - 1);
    d->text[sizeof(d->text) - 1] = '\0';
    resize(seg);
    lv_obj_invalidate(seg);
}

void seg_set_color(lv_obj_t *seg, lv_color_t color)
{
    SegData *d = (SegData *)lv_obj_get_user_data(seg);
    if (lv_color_eq(d->color, color)) {
        return;
    }
    d->color = color;
    lv_obj_invalidate(seg);
}

void seg_set_height(lv_obj_t *seg, lv_coord_t digit_height)
{
    SegData *d = (SegData *)lv_obj_get_user_data(seg);
    if (d->h == digit_height) {
        return;
    }
    d->h = digit_height;
    resize(seg);
    lv_obj_invalidate(seg);
}

void seg_set_opa(lv_obj_t *seg, lv_opa_t opa)
{
    SegData *d = (SegData *)lv_obj_get_user_data(seg);
    if (d->opa == opa) {
        return;
    }
    d->opa = opa;
    lv_obj_invalidate(seg);
}
