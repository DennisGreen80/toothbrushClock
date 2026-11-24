// TCA9554 8-bit I2C IO expander (LCD/touch reset, LCD chip select, buzzer).
#pragma once

#include <stdint.h>

bool exio_init();
void exio_write(uint8_t bit, bool level);
