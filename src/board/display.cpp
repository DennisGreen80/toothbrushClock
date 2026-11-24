#include "board/display.h"

#include <string.h>

#include "board/exio.h"
#include "board/hal.h"
#include "board/pins.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "sdkconfig.h"

namespace {

constexpr const char *TAG = "display";

// Refresh rate = pclk / (548 * 508) with the timings below: 16 MHz ~57 Hz, 18 MHz ~65 Hz.
// The frame buffer lives in PSRAM; bounce buffers in internal RAM decouple the LCD from PSRAM
// bandwidth spikes, which is what limited the Arduino build to 16 MHz (sideways drift above that).
// Both are set in menuconfig ("Toothbrush timer").
constexpr uint32_t PIXEL_CLOCK_HZ = CONFIG_TOOTHBRUSH_LCD_PCLK_MHZ * 1000 * 1000;
constexpr uint32_t BOUNCE_BUFFER_PX = CONFIG_TOOTHBRUSH_LCD_BOUNCE_LINES * LCD_WIDTH;

constexpr uint32_t BACKLIGHT_FREQ_HZ = 20000;
constexpr ledc_timer_bit_t BACKLIGHT_RES = LEDC_TIMER_10_BIT;
constexpr ledc_channel_t BACKLIGHT_CHANNEL = LEDC_CHANNEL_0;
constexpr ledc_timer_t BACKLIGHT_TIMER = LEDC_TIMER_0;

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
    delay_ms(10);
    exio_write(EXIO_LCD_RST, true);
    delay_ms(120);

    exio_write(EXIO_LCD_CS, false);
    delay_ms(5);
    for (const InitCmd &c : kInitCmds) {
        spi_write9(false, c.cmd);
        for (uint8_t i = 0; i < c.len; i++) {
            spi_write9(true, c.data[i]);
        }
        if (c.delay_ms) {
            delay_ms(c.delay_ms);
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
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
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
    cfg.bits_per_pixel = 16;
    cfg.num_fbs = 1;
    // Two bounce buffers of this size in internal RAM; the LCD DMA reads those while an ISR refills
    // them from the PSRAM frame buffer. Must divide the frame size (480 * 480 pixels).
    cfg.bounce_buffer_size_px = BOUNCE_BUFFER_PX;
    cfg.dma_burst_size = 64;
    cfg.hsync_gpio_num = (gpio_num_t)PIN_LCD_HSYNC;
    cfg.vsync_gpio_num = (gpio_num_t)PIN_LCD_VSYNC;
    cfg.de_gpio_num = (gpio_num_t)PIN_LCD_DE;
    cfg.pclk_gpio_num = (gpio_num_t)PIN_LCD_PCLK;
    cfg.disp_gpio_num = GPIO_NUM_NC;
    const int data_pins[16] = PIN_LCD_DATA;
    for (int i = 0; i < 16; i++) {
        cfg.data_gpio_nums[i] = (gpio_num_t)data_pins[i];
    }
    cfg.flags.fb_in_psram = 1;

    const esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &s_panel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_lcd_new_rgb_panel: %s", esp_err_to_name(err));
        return false;
    }
    esp_lcd_panel_reset(s_panel);
    esp_lcd_panel_init(s_panel);
    ESP_LOGI(TAG, "RGB panel %d MHz, bounce buffers %d lines", CONFIG_TOOTHBRUSH_LCD_PCLK_MHZ,
             CONFIG_TOOTHBRUSH_LCD_BOUNCE_LINES);
    return true;
}

} // namespace

bool display_init()
{
    if (!st7701_init()) {
        ESP_LOGE(TAG, "ST7701 SPI init failed");
        return false;
    }
    if (!rgb_panel_init()) {
        ESP_LOGE(TAG, "RGB panel init failed");
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
    ledc_timer_config_t timer;
    memset(&timer, 0, sizeof(timer));
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = BACKLIGHT_RES;
    timer.timer_num = BACKLIGHT_TIMER;
    timer.freq_hz = BACKLIGHT_FREQ_HZ;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);

    ledc_channel_config_t channel;
    memset(&channel, 0, sizeof(channel));
    channel.gpio_num = PIN_LCD_BACKLIGHT;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = BACKLIGHT_CHANNEL;
    channel.timer_sel = BACKLIGHT_TIMER;
    channel.duty = 0;
    ledc_channel_config(&channel);
}

void backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t max_duty = (1u << BACKLIGHT_RES) - 1;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL, max_duty * percent / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL);
}
