// Pin map for the Waveshare ESP32-S3-Touch-LCD-2.8C (from the board schematic).
#pragma once

// Shared I2C bus: GT911 touch, PCF85063 RTC, QMI8658 IMU, TCA9554 IO expander
#define PIN_I2C_SDA 15
#define PIN_I2C_SCL 7
#define I2C_FREQ_HZ 400000

// ST7701 3-wire SPI (init only; CS is on the IO expander)
#define PIN_LCD_SPI_SDA 1
#define PIN_LCD_SPI_SCK 2

// RGB565 parallel interface
#define PIN_LCD_HSYNC 38
#define PIN_LCD_VSYNC 39
#define PIN_LCD_DE 40
#define PIN_LCD_PCLK 41
// data0..4 = B1..B5, data5..10 = G0..G5, data11..15 = R1..R5
#define PIN_LCD_DATA {5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17}

#define PIN_LCD_BACKLIGHT 6
#define PIN_TOUCH_INT 16

// TCA9554 expander outputs (bit numbers, P0..P7)
#define EXIO_LCD_RST 0
#define EXIO_TOUCH_RST 1
#define EXIO_LCD_CS 2
#define EXIO_SD_CS 3
#define EXIO_IMU_INT2 4
#define EXIO_IMU_INT1 5
#define EXIO_RTC_INT 6
#define EXIO_BUZZER 7

#define I2C_ADDR_TCA9554 0x20
#define I2C_ADDR_GT911 0x5D
#define I2C_ADDR_PCF85063 0x51

#define LCD_WIDTH 480
#define LCD_HEIGHT 480
