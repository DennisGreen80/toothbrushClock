#include "board/rtc_pcf85063.h"

#include <Arduino.h>
#include <Wire.h>

#include "board/pins.h"

namespace {

constexpr uint8_t REG_CONTROL_1 = 0x00;
constexpr uint8_t REG_SECONDS = 0x04;
constexpr uint8_t SECONDS_OS_FLAG = 0x80; // oscillator stopped, time integrity not guaranteed
constexpr uint8_t CONTROL_1_CAP_12PF5 = 0x01;
constexpr int YEAR_BASE = 2000;

uint8_t to_bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
int from_bcd(uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); }

} // namespace

bool rtc_init()
{
    Wire.beginTransmission(I2C_ADDR_PCF85063);
    Wire.write(REG_CONTROL_1);
    Wire.write(CONTROL_1_CAP_12PF5); // running, 24 h mode
    bool ok = Wire.endTransmission() == 0;
    if (!ok) {
        Serial.println("[rtc] PCF85063 not responding");
    }
    return ok;
}

bool rtc_read(struct tm *out)
{
    Wire.beginTransmission(I2C_ADDR_PCF85063);
    Wire.write(REG_SECONDS);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    uint8_t b[7];
    if (Wire.requestFrom((uint8_t)I2C_ADDR_PCF85063, (uint8_t)7) != 7) {
        return false;
    }
    for (int i = 0; i < 7; i++) {
        b[i] = Wire.read();
    }

    memset(out, 0, sizeof(*out));
    out->tm_sec = from_bcd(b[0] & 0x7F);
    out->tm_min = from_bcd(b[1] & 0x7F);
    out->tm_hour = from_bcd(b[2] & 0x3F);
    out->tm_mday = from_bcd(b[3] & 0x3F);
    out->tm_wday = b[4] & 0x07;
    out->tm_mon = from_bcd(b[5] & 0x1F) - 1;
    out->tm_year = from_bcd(b[6]) + YEAR_BASE - 1900;
    out->tm_isdst = -1;
    return (b[0] & SECONDS_OS_FLAG) == 0;
}

bool rtc_write(const struct tm *t)
{
    // Writing the seconds register also clears the oscillator-stopped flag.
    Wire.beginTransmission(I2C_ADDR_PCF85063);
    Wire.write(REG_SECONDS);
    Wire.write(to_bcd(t->tm_sec) & 0x7F);
    Wire.write(to_bcd(t->tm_min));
    Wire.write(to_bcd(t->tm_hour));
    Wire.write(to_bcd(t->tm_mday));
    Wire.write((uint8_t)(t->tm_wday & 0x07));
    Wire.write(to_bcd(t->tm_mon + 1));
    Wire.write(to_bcd((t->tm_year + 1900 - YEAR_BASE) % 100));
    return Wire.endTransmission() == 0;
}
