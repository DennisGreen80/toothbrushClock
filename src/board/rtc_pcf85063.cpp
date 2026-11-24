#include "board/rtc_pcf85063.h"

#include <string.h>

#include "board/i2c_bus.h"
#include "board/pins.h"
#include "esp_log.h"

namespace {

constexpr uint8_t REG_CONTROL_1 = 0x00;
constexpr uint8_t REG_SECONDS = 0x04;
constexpr uint8_t SECONDS_OS_FLAG = 0x80; // oscillator stopped, time integrity not guaranteed
constexpr uint8_t CONTROL_1_CAP_12PF5 = 0x01;
constexpr int YEAR_BASE = 2000;

i2c_master_dev_handle_t s_dev = nullptr;

uint8_t to_bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
int from_bcd(uint8_t v) { return ((v >> 4) * 10) + (v & 0x0F); }

} // namespace

bool rtc_init()
{
    if (!s_dev) {
        s_dev = i2c_bus_add(I2C_ADDR_PCF85063);
    }
    const uint8_t control = CONTROL_1_CAP_12PF5; // running, 24 h mode
    const bool ok = i2c_write_reg(s_dev, REG_CONTROL_1, 1, &control, 1);
    if (!ok) {
        ESP_LOGE("rtc", "PCF85063 not responding");
    }
    return ok;
}

bool rtc_read(struct tm *out)
{
    uint8_t b[7];
    if (!i2c_read_reg(s_dev, REG_SECONDS, 1, b, sizeof(b))) {
        return false;
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
    const uint8_t b[7] = {
        (uint8_t)(to_bcd(t->tm_sec) & 0x7F), to_bcd(t->tm_min), to_bcd(t->tm_hour), to_bcd(t->tm_mday),
        (uint8_t)(t->tm_wday & 0x07), to_bcd(t->tm_mon + 1), to_bcd((t->tm_year + 1900 - YEAR_BASE) % 100),
    };
    return i2c_write_reg(s_dev, REG_SECONDS, 1, b, sizeof(b));
}
