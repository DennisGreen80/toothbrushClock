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
    const char *reindeer;
    const char *heart;
    const char *candy;
    const char *ok;
    const char *days_to_december;  /* "dage til december" (after the number) */
    const char *day_to_december;   /* singular: "1 dag til december" */
    const char *days_to_christmas; /* "dage til jul" */
    const char *day_to_christmas;
    const char *merry_christmas;   /* shown 24-26 December instead of a number */
    const char *quadrants[4]; /* upper right, lower right, lower left, upper left */
    const char *weekdays[7];  /* Sunday first, like struct tm */
    const char *months[12];
} brush_strings_t;

const brush_strings_t *brush_strings(brush_lang_t lang);
/* "Monday, 23 Nov" / "Mandag 23. nov." */
void brush_format_date(brush_lang_t lang, int wday, int mday, int mon, char *out, size_t len);

typedef enum {
    BRUSH_XMAS_TO_DECEMBER = 0, /* counting down to 1 December */
    BRUSH_XMAS_TO_CHRISTMAS,    /* 1-23 December: counting down to Christmas Eve */
    BRUSH_XMAS_MERRY,           /* 24-26 December */
} brush_xmas_phase_t;

/* Christmas countdown for a date (`mon` 0-11 like struct tm). Counts the days to 1 December, then
 * to Christmas Eve (24 December); 24-26 December is BRUSH_XMAS_MERRY (days = 0), and from
 * 27 December it counts to 1 December of the next year. */
brush_xmas_phase_t brush_xmas_countdown(int year, int mon, int mday, int *days);
/* Text shown with the number ("dage til jul", singular for 1), or the greeting for MERRY. */
const char *brush_xmas_text(brush_lang_t lang, brush_xmas_phase_t phase, int days);

#ifdef __cplusplus
}
#endif

#endif /* BRUSH_I18N_H */
