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
