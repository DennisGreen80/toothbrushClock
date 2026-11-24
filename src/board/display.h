// ST7701 round 480x480 RGB panel and PWM backlight.
#pragma once

#include <stdint.h>

bool display_init();
// Copies an area (inclusive coordinates) of RGB565 pixels into the frame buffer.
void display_flush(int x1, int y1, int x2, int y2, const void *pixels);

void backlight_init();
void backlight_set(uint8_t percent);
