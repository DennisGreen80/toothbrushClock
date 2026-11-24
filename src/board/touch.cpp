#include "board/touch.h"

#include "board/exio.h"
#include "board/hal.h"
#include "board/i2c_bus.h"
#include "board/pins.h"
#include "driver/gpio.h"
#include "esp_log.h"

namespace {

constexpr const char *TAG = "touch";
constexpr uint16_t REG_STATUS = 0x814E;
constexpr uint16_t REG_POINT1 = 0x8150;
constexpr uint16_t REG_PRODUCT_ID = 0x8140;

i2c_master_dev_handle_t s_dev = nullptr;
bool s_present = false;
bool s_last_pressed = false;
uint16_t s_last_x = 0;
uint16_t s_last_y = 0;
uint8_t s_empty_samples = 0;
// A single empty sample mid-swipe would end the gesture early; require this many in a row.
constexpr uint8_t RELEASE_SAMPLES = 2;

bool read_regs(uint16_t reg, uint8_t *buf, uint8_t len) { return i2c_read_reg(s_dev, reg, 2, buf, len); }

void write_reg(uint16_t reg, uint8_t value) { i2c_write_reg(s_dev, reg, 2, &value, 1); }

} // namespace

bool touch_init()
{
    // Holding INT low while releasing reset selects I2C address 0x5D.
    const gpio_num_t int_pin = (gpio_num_t)PIN_TOUCH_INT;
    gpio_reset_pin(int_pin);
    gpio_set_direction(int_pin, GPIO_MODE_OUTPUT);
    gpio_set_level(int_pin, 0);
    exio_write(EXIO_TOUCH_RST, false);
    delay_ms(10);
    exio_write(EXIO_TOUCH_RST, true);
    delay_ms(60);
    gpio_set_direction(int_pin, GPIO_MODE_INPUT);
    delay_ms(100);

    if (!s_dev) {
        s_dev = i2c_bus_add(I2C_ADDR_GT911);
    }
    uint8_t id[4] = {0};
    s_present = read_regs(REG_PRODUCT_ID, id, 4);
    if (s_present) {
        ESP_LOGI(TAG, "GT%c%c%c%c found", id[0], id[1], id[2], id[3]);
    } else {
        ESP_LOGE(TAG, "GT911 not found");
    }
    return s_present;
}

bool touch_read(uint16_t *x, uint16_t *y)
{
    if (!s_present) {
        return false;
    }
    uint8_t status = 0;
    if (!read_regs(REG_STATUS, &status, 1) || !(status & 0x80)) {
        // Bus hiccup or no new sample yet: report the previous state so a held finger stays pressed.
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
        } else {
            pressed = s_last_pressed;
        }
    }
    write_reg(REG_STATUS, 0); // acknowledge the sample
    if (pressed) {
        s_empty_samples = 0;
    } else if (s_last_pressed && ++s_empty_samples < RELEASE_SAMPLES) {
        pressed = true;
    }
    s_last_pressed = pressed;
    *x = s_last_x;
    *y = s_last_y;
    return pressed;
}
