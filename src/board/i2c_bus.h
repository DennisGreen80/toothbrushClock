// Shared I2C master bus (GT911 touch, PCF85063 RTC, TCA9554 IO expander).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"

bool i2c_bus_init();
// Adds a 7-bit device at the bus speed from pins.h; returns nullptr on failure.
i2c_master_dev_handle_t i2c_bus_add(uint8_t address);

// Register helpers; `reg_len` is 1 or 2 bytes (big-endian register address, as GT911 uses).
bool i2c_write_reg(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t reg_len, const uint8_t *data, size_t len);
bool i2c_read_reg(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t reg_len, uint8_t *data, size_t len);
