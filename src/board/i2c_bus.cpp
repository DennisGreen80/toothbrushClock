#include "board/i2c_bus.h"

#include <string.h>

#include "board/pins.h"
#include "esp_log.h"

namespace {

constexpr const char *TAG = "i2c";
constexpr int TIMEOUT_MS = 50;

i2c_master_bus_handle_t s_bus = nullptr;

} // namespace

bool i2c_bus_init()
{
    if (s_bus) {
        return true;
    }
    i2c_master_bus_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = (gpio_num_t)PIN_I2C_SDA;
    cfg.scl_io_num = (gpio_num_t)PIN_I2C_SCL;
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    const esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
        s_bus = nullptr;
        return false;
    }
    return true;
}

i2c_master_dev_handle_t i2c_bus_add(uint8_t address)
{
    if (!s_bus) {
        return nullptr;
    }
    i2c_device_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    cfg.device_address = address;
    cfg.scl_speed_hz = I2C_FREQ_HZ;
    i2c_master_dev_handle_t dev = nullptr;
    if (i2c_master_bus_add_device(s_bus, &cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "cannot add device 0x%02X", address);
        return nullptr;
    }
    return dev;
}

bool i2c_write_reg(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t reg_len, const uint8_t *data, size_t len)
{
    if (!dev) {
        return false;
    }
    uint8_t buf[16];
    if (reg_len + len > sizeof(buf)) {
        return false;
    }
    size_t n = 0;
    if (reg_len == 2) {
        buf[n++] = (uint8_t)(reg >> 8);
    }
    buf[n++] = (uint8_t)reg;
    if (len) {
        memcpy(buf + n, data, len);
    }
    return i2c_master_transmit(dev, buf, n + len, TIMEOUT_MS) == ESP_OK;
}

bool i2c_read_reg(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t reg_len, uint8_t *data, size_t len)
{
    if (!dev) {
        return false;
    }
    uint8_t addr[2];
    size_t n = 0;
    if (reg_len == 2) {
        addr[n++] = (uint8_t)(reg >> 8);
    }
    addr[n++] = (uint8_t)reg;
    return i2c_master_transmit_receive(dev, addr, n, data, len, TIMEOUT_MS) == ESP_OK;
}
