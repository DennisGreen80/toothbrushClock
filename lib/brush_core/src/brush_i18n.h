/* User-visible strings (UTF-8) for English and Danish. Mirrored in preview/sim.js. */
#ifndef BRUSH_I18N_H
#define BRUSH_I18N_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BRUSH_LANG_EN = 0,
    BRUSH_LANG_DA,
    BRUSH_LANG_COUNT,
} brush_lang_t;

typedef struct {
    const char *language; /* the language's own name, shown on the language button */
    const char *hint;
    const char *paused;
    const char *done;
    const char *too_hard; /* Oral-B high pressure warning */
    const char *settings;
    const char *countdown;
    const char *count_up;
    const char *sound;
    const char *muted;
    const char *clock_face;
    const char *classic;
    const char *christmas;
    const char *ok;
    const char *quadrants[4]; /* upper right, lower right, lower left, upper left */
    const char *weekdays[7];  /* Sunday first, like struct tm */
    const char *months[12];
} brush_strings_t;

const brush_strings_t *brush_strings(brush_lang_t lang);
/* "Monday, 23 Nov" / "Mandag 23. nov." */
void brush_format_date(brush_lang_t lang, int wday, int mday, int mon, char *out, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BRUSH_I18N_H */
