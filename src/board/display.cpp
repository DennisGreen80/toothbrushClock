#include "board/display.h"

#include <Arduino.h>
#include <string.h>

#include "board/exio.h"
#include "board/pins.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

namespace {

// Refresh rate = pclk / (548 * 508) with the timings below. ~29 Hz (8 MHz) made colored
// backgrounds visibly pulse in intensity (LCD polarity inversion); 16 MHz gives ~57 Hz.
// If the image ever shifts or tears (PSRAM bandwidth), lower it via build_flags, e.g. -DLCD_PIXEL_CLOCK_MHZ=14.
#ifndef LCD_PIXEL_CLOCK_MHZ
#define LCD_PIXEL_CLOCK_MHZ 16
#endif
constexpr uint32_t PIXEL_CLOCK_HZ = LCD_PIXEL_CLOCK_MHZ * 1000 * 1000;

constexpr uint32_t BACKLIGHT_FREQ_HZ = 20000;
constexpr uint8_t BACKLIGHT_RES_BITS = 10;
constexpr uint8_t BACKLIGHT_CHANNEL = 1;

spi_device_handle_t s_spi = nullptr;
esp_lcd_panel_handle_t s_panel = nullptr;

struct InitCmd {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[16];
    uint16_t delay_ms;
};

// Vendor init sequence for the 2.8" round ST7701 panel (Waveshare demo code).
const InitCmd kInitCmds[] = {
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x13}, 0},
    {0xEF, 1, {0x08}, 0},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x10}, 0},
    {0xC0, 2, {0x3B, 0x00}, 0},
    {0xC1, 2, {0x10, 0x0C}, 0},
    {0xC2, 2, {0x07, 0x0A}, 0},
    {0xC7, 1, {0x00}, 0},
    {0xCC, 1, {0x10}, 0},
    {0xCD, 1, {0x08}, 0},
    {0xB0, 16, {0x05, 0x12, 0x98, 0x0E, 0x0F, 0x07, 0x07, 0x09, 0x09, 0x23, 0x05, 0x52, 0x0F, 0x67, 0x2C, 0x11}, 0},
    {0xB1, 16, {0x0B, 0x11, 0x97, 0x0C, 0x12, 0x06, 0x06, 0x08, 0x08, 0x22, 0x03, 0x51, 0x11, 0x66, 0x2B, 0x0F}, 0},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x11}, 0},
    {0xB0, 1, {0x5D}, 0},
    {0xB1, 1, {0x3E}, 0},
    {0xB2, 1, {0x81}, 0},
    {0xB3, 1, {0x80}, 0},
    {0xB5, 1, {0x4E}, 0},
    {0xB7, 1, {0x85}, 0},
    {0xB8, 1, {0x20}, 0},
    {0xC1, 1, {0x78}, 0},
    {0xC2, 1, {0x78}, 0},
    {0xD0, 1, {0x88}, 0},
    {0xE0, 3, {0x00, 0x00, 0x02}, 0},
    {0xE1, 11, {0x06, 0x30, 0x08, 0x30, 0x05, 0x30, 0x07, 0x30, 0x00, 0x33, 0x33}, 0},
    {0xE2, 12, {0x11, 0x11, 0x33, 0x33, 0xF4, 0x00, 0x00, 0x00, 0xF4, 0x00, 0x00, 0x00}, 0},
    {0xE3, 4, {0x00, 0x00, 0x11, 0x11}, 0},
    {0xE4, 2, {0x44, 0x44}, 0},
    {0xE5, 16, {0x0D, 0xF5, 0x30, 0xF0, 0x0F, 0xF7, 0x30, 0xF0, 0x09, 0xF1, 0x30, 0xF0, 0x0B, 0xF3, 0x30, 0xF0}, 0},
    {0xE6, 4, {0x00, 0x00, 0x11, 0x11}, 0},
    {0xE7, 2, {0x44, 0x44}, 0},
    {0xE8, 16, {0x0C, 0xF4, 0x30, 0xF0, 0x0E, 0xF6, 0x30, 0xF0, 0x08, 0xF0, 0x30, 0xF0, 0x0A, 0xF2, 0x30, 0xF0}, 0},
    {0xE9, 2, {0x36, 0x01}, 0},
    {0xEB, 7, {0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40}, 0},
    {0xED, 16, {0xFF, 0x10, 0xAF, 0x76, 0x54, 0x2B, 0xCF, 0xFF, 0xFF, 0xFC, 0xB2, 0x45, 0x67, 0xFA, 0x01, 0xFF}, 0},
    {0xEF, 6, {0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 0},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}, 0},
    {0x11, 0, {0}, 120}, // sleep out
    {0x3A, 1, {0x66}, 0},
    {0x36, 1, {0x00}, 0},
    {0x35, 1, {0x00}, 0},
    {0x29, 0, {0}, 20}, // display on
};

// 3-wire SPI: the 9th (first) bit selects command (0) or data (1).
void spi_write9(bool is_data, uint8_t value)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.cmd = is_data ? 1 : 0;
    t.addr = value;
    spi_device_transmit(s_spi, &t);
}

bool st7701_init()
{
    spi_bus_config_t bus;
    memset(&bus, 0, sizeof(bus));
    bus.mosi_io_num = PIN_LCD_SPI_SDA;
    bus.miso_io_num = -1;
    bus.sclk_io_num = PIN_LCD_SPI_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 64;
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        return false;
    }

    spi_device_interface_config_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.command_bits = 1;
    dev.address_bits = 8;
    dev.mode = 0;
    dev.clock_speed_hz = 10 * 1000 * 1000;
    dev.spics_io_num = -1; // CS is driven through the IO expander
    dev.queue_size = 1;
    if (spi_bus_add_device(SPI2_HOST, &dev, &s_spi) != ESP_OK) {
        return false;
    }

    exio_write(EXIO_LCD_RST, false);
    delay(10);
    exio_write(EXIO_LCD_RST, true);
    delay(120);

    exio_write(EXIO_LCD_CS, false);
    delay(5);
    for (const InitCmd &c : kInitCmds) {
        spi_write9(false, c.cmd);
        for (uint8_t i = 0; i < c.len; i++) {
            spi_write9(true, c.data[i]);
        }
        if (c.delay_ms) {
            delay(c.delay_ms);
        }
    }
    exio_write(EXIO_LCD_CS, true);

    // The SPI pins are shared with the SD card slot; release them.
    spi_bus_remove_device(s_spi);
    spi_bus_free(SPI2_HOST);
    s_spi = nullptr;
    return true;
}

bool rgb_panel_init()
{
    esp_lcd_rgb_panel_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.clk_src = LCD_CLK_SRC_PLL160M;
    cfg.timings.pclk_hz = PIXEL_CLOCK_HZ;
    cfg.timings.h_res = LCD_WIDTH;
    cfg.timings.v_res = LCD_HEIGHT;
    cfg.timings.hsync_pulse_width = 8;
    cfg.timings.hsync_back_porch = 10;
    cfg.timings.hsync_front_porch = 50;
    cfg.timings.vsync_pulse_width = 2;
    cfg.timings.vsync_back_porch = 18;
    cfg.timings.vsync_front_porch = 8;
    cfg.timings.flags.pclk_active_neg = 0;
    cfg.data_width = 16;
    cfg.psram_trans_align = 64;
    cfg.hsync_gpio_num = PIN_LCD_HSYNC;
    cfg.vsync_gpio_num = PIN_LCD_VSYNC;
    cfg.de_gpio_num = PIN_LCD_DE;
    cfg.pclk_gpio_num = PIN_LCD_PCLK;
    cfg.disp_gpio_num = -1;
    const int data_pins[16] = PIN_LCD_DATA;
    for (int i = 0; i < 16; i++) {
        cfg.data_gpio_nums[i] = data_pins[i];
    }
    cfg.flags.fb_in_psram = 1;

    if (esp_lcd_new_rgb_panel(&cfg, &s_panel) != ESP_OK) {
        return false;
    }
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    return true;
}

} // namespace

bool display_init()
{
    if (!st7701_init()) {
        Serial.println("[display] ST7701 SPI init failed");
        return false;
    }
    if (!rgb_panel_init()) {
        Serial.println("[display] RGB panel init failed");
        return false;
    }
    return true;
}

void display_flush(int x1, int y1, int x2, int y2, const void *pixels)
{
    esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2 + 1, y2 + 1, pixels);
}

void backlight_init()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PIN_LCD_BACKLIGHT, BACKLIGHT_FREQ_HZ, BACKLIGHT_RES_BITS);
#else
    ledcSetup(BACKLIGHT_CHANNEL, BACKLIGHT_FREQ_HZ, BACKLIGHT_RES_BITS);
    ledcAttachPin(PIN_LCD_BACKLIGHT, BACKLIGHT_CHANNEL);
#endif
    backlight_set(0);
}

void backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t max_duty = (1u << BACKLIGHT_RES_BITS) - 1;
    uint32_t duty = max_duty * percent / 100;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PIN_LCD_BACKLIGHT, duty);
#else
    ledcWrite(BACKLIGHT_CHANNEL, duty);
#endif
}
