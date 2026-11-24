#include "board/exio.h"

#include "board/i2c_bus.h"
#include "board/pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {

constexpr uint8_t REG_OUTPUT = 0x01;
constexpr uint8_t REG_CONFIG = 0x03; // 1 = input

i2c_master_dev_handle_t s_dev = nullptr;
// Shadow of the output register so a pin change is a single I2C write.
uint8_t s_output = 0;
// exio_write() is called from the UI task and the buzzer task.
SemaphoreHandle_t s_lock = nullptr;

bool write_reg(uint8_t reg, uint8_t value) { return i2c_write_reg(s_dev, reg, 1, &value, 1); }

} // namespace

bool exio_init()
{
    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
    }
    if (!s_dev) {
        s_dev = i2c_bus_add(I2C_ADDR_TCA9554);
    }
    // Resets and chip selects idle high, buzzer off.
    s_output = (1 << EXIO_LCD_RST) | (1 << EXIO_TOUCH_RST) | (1 << EXIO_LCD_CS) | (1 << EXIO_SD_CS);
    bool ok = write_reg(REG_OUTPUT, s_output);
    // Interrupt lines from the IMU and RTC stay inputs; everything else drives.
    const uint8_t inputs = (1 << EXIO_IMU_INT2) | (1 << EXIO_IMU_INT1) | (1 << EXIO_RTC_INT);
    ok &= write_reg(REG_CONFIG, inputs);
    if (!ok) {
        ESP_LOGE("exio", "TCA9554 not responding");
    }
    return ok;
}

void exio_write(uint8_t bit, bool level)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint8_t next = level ? (s_output | (1 << bit)) : (s_output & ~(1 << bit));
    if (next != s_output) {
        s_output = next;
        write_reg(REG_OUTPUT, s_output);
    }
    xSemaphoreGive(s_lock);
}
