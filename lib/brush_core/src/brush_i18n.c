#include "brush_i18n.h"

#include <stdio.h>

/* Only ASCII plus Æ Ø Å æ ø å are available in the display fonts (src/ui/fonts). */
static const brush_strings_t kStrings[BRUSH_LANG_COUNT] = {
    [BRUSH_LANG_EN] = {
        .language = "English",
        .hint = "Tap to start brushing",
        .paused = "Paused",
        .done = "Done!",
        .too_hard = "Too hard",
        .settings = "Settings",
        .countdown = "Countdown",
        .count_up = "Count up",
        .sound = "Sound",
        .muted = "Muted",
        .clock_face = "Face",
        .classic = "Classic",
        .christmas = "Christmas",
        .reindeer = "Reindeer",
        .heart = "Woven Heart",
        .candy = "Candy Cane",
        .ok = "Done",
        .days_to_december = "days until December",
        .day_to_december = "day until December",
        .days_to_christmas = "days until Christmas",
        .day_to_christmas = "day until Christmas",
        .merry_christmas = "Merry Christmas",
        .quadrants = {"Upper right", "Lower right", "Lower left", "Upper left"},
        .weekdays = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
        .months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},
    },
    [BRUSH_LANG_DA] = {
        .language = "Dansk",
        .hint = "Tryk for at børste",
        .paused = "Pause",
        .done = "Færdig!",
        .too_hard = "For hårdt",
        .settings = "Indstillinger",
        .countdown = "Nedtælling",
        .count_up = "Tæl op",
        .sound = "Lyd",
        .muted = "Lydløs",
        .clock_face = "Urskive",
        .classic = "Klassisk",
        .christmas = "Jul",
        .reindeer = "Rensdyr",
        .heart = "Julehjerte",
        .candy = "Slikstok",
        .ok = "Færdig",
        .days_to_december = "dage til december",
        .day_to_december = "dag til december",
        .days_to_christmas = "dage til jul",
        .day_to_christmas = "dag til jul",
        .merry_christmas = "Glædelig jul",
        .quadrants = {"Øverst højre", "Nederst højre", "Nederst venstre", "Øverst venstre"},
        .weekdays = {"Søndag", "Mandag", "Tirsdag", "Onsdag", "Torsdag", "Fredag", "Lørdag"},
        .months = {"jan.", "feb.", "mar.", "apr.", "maj", "jun.", "jul.", "aug.", "sep.", "okt.", "nov.", "dec."},
    },
};

const brush_strings_t *brush_strings(brush_lang_t lang)
{
    return &kStrings[(unsigned)lang < BRUSH_LANG_COUNT ? lang : BRUSH_LANG_EN];
}

void brush_format_date(brush_lang_t lang, int wday, int mday, int mon, char *out, size_t len)
{
    const brush_strings_t *s = brush_strings(lang);
    const char *weekday = s->weekdays[((wday % 7) + 7) % 7];
    const char *month = s->months[((mon % 12) + 12) % 12];
    if (lang == BRUSH_LANG_DA) {
        snprintf(out, len, "%s %d. %s", weekday, mday, month);
    } else {
        snprintf(out, len, "%s, %d %s", weekday, mday, month);
    }
}

/* Days since 1970-01-01 in the proleptic Gregorian calendar (month 1-12). */
static long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

brush_xmas_phase_t brush_xmas_countdown(int year, int mon, int mday, int *days)
{
    if (mon == 11 && mday >= 24 && mday <= 26) {
        *days = 0;
        return BRUSH_XMAS_MERRY;
    }
    if (mon == 11 && mday < 24) {
        *days = 24 - mday;
        return BRUSH_XMAS_TO_CHRISTMAS;
    }
    const int target_year = mon == 11 ? year + 1 : year;
    *days = (int)(days_from_civil(target_year, 12, 1) - days_from_civil(year, mon + 1, mday));
    return BRUSH_XMAS_TO_DECEMBER;
}

const char *brush_xmas_text(brush_lang_t lang, brush_xmas_phase_t phase, int days)
{
    const brush_strings_t *s = brush_strings(lang);
    switch (phase) {
    case BRUSH_XMAS_TO_CHRISTMAS: return days == 1 ? s->day_to_christmas : s->days_to_christmas;
    case BRUSH_XMAS_MERRY: return s->merry_christmas;
    case BRUSH_XMAS_TO_DECEMBER:
    default: return days == 1 ? s->day_to_december : s->days_to_december;
    }
}
