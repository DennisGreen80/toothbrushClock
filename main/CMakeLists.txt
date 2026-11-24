# The application sources live in ../src (shared paths with the README and preview/).
idf_component_register(
    SRC_DIRS "../src" "../src/board" "../src/ui" "../src/ui/fonts"
    INCLUDE_DIRS "../src"
    REQUIRES
        brush_core
        bt
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_ledc
        esp_driver_spi
        esp_lcd
        esp_timer
        nvs_flash
)
