// GT911 capacitive touch controller (polled over I2C).
#pragma once

#include <stdint.h>

bool touch_init();
// Returns true while the panel is touched and stores the first touch point.
bool touch_read(uint16_t *x, uint16_t *y);
