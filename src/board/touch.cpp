#include "board/touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "board/exio.h"
#include "board/pins.h"

namespace {

constexpr uint16_t REG_STATUS = 0x814E;
constexpr uint16_t REG_POINT1 = 0x8150;
constexpr uint16_t REG_PRODUCT_ID = 0x8140;

bool s_present = false;
bool s_last_pressed = false;
uint16_t s_last_x = 0;
uint16_t s_last_y = 0;

bool read_regs(uint16_t reg, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(I2C_ADDR_GT911);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    if (Wire.requestFrom((uint8_t)I2C_ADDR_GT911, len) != len) {
        return false;
    }
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return true;
}

void write_reg(uint16_t reg, uint8_t value)
{
    Wire.beginTransmission(I2C_ADDR_GT911);
    Wire.write(reg >> 8);
    Wire.write(reg & 0xFF);
    Wire.write(value);
    Wire.endTransmission();
}

} // namespace

bool touch_init()
{
    // Holding INT low while releasing reset selects I2C address 0x5D.
    pinMode(PIN_TOUCH_INT, OUTPUT);
    digitalWrite(PIN_TOUCH_INT, LOW);
    exio_write(EXIO_TOUCH_RST, false);
    delay(10);
    exio_write(EXIO_TOUCH_RST, true);
    delay(60);
    pinMode(PIN_TOUCH_INT, INPUT);
    delay(100);

    uint8_t id[4] = {0};
    s_present = read_regs(REG_PRODUCT_ID, id, 4);
    if (s_present) {
        Serial.printf("[touch] GT%c%c%c%c found\n", id[0], id[1], id[2], id[3]);
    } else {
        Serial.println("[touch] GT911 not found");
    }
    return s_present;
}

bool touch_read(uint16_t *x, uint16_t *y)
{
    if (!s_present) {
        return false;
    }
    uint8_t status = 0;
    if (!read_regs(REG_STATUS, &status, 1)) {
        return false;
    }
    if (!(status & 0x80)) {
        // No new sample yet: report the previous state so a held finger stays pressed.
        *x = s_last_x;
        *y = s_last_y;
        return s_last_pressed;
    }

    uint8_t points = status & 0x0F;
    bool pressed = false;
    if (points > 0 && points <= 5) {
        uint8_t p[4];
        if (read_regs(REG_POINT1, p, sizeof(p))) {
            s_last_x = (uint16_t)(p[0] | (p[1] << 8));
            s_last_y = (uint16_t)(p[2] | (p[3] << 8));
            pressed = true;
        }
    }
    write_reg(REG_STATUS, 0); // acknowledge the sample
    s_last_pressed = pressed;
    *x = s_last_x;
    *y = s_last_y;
    return pressed;
}
