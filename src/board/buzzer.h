// Buzzer sound design. The board has an active (fixed-pitch) buzzer switched through the
// IO expander, so all character comes from rhythm. Patterns play on a dedicated FreeRTOS
// task so their timing stays crisp while the UI is busy rendering animations.
#pragma once

#include <stdint.h>

enum class Beep : uint8_t {
    Start,  // one short pip
    Mark1,  // 0:30 -> one pip
    Mark2,  // 1:00 -> two pips
    Mark3,  // 1:30 -> three pips
    Done,   // 2:00 -> two longer pips
};

// Beep for the n-th 30 s mark (1-based).
Beep buzzer_mark(uint8_t n);

void buzzer_init();
void buzzer_enable(bool enabled);
void buzzer_play(Beep beep);
