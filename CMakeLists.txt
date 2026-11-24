# Toothbrush timer for the Waveshare ESP32-S3-Touch-LCD-2.8C (ESP-IDF 6.1 + LVGL 9).
#
#   pio run -e waveshare_28c -t upload       (pioarduino, see platformio.ini)
#   idf.py set-target esp32s3 && idf.py build flash monitor
cmake_minimum_required(VERSION 3.22)

# The timer logic is a plain C library shared with the PlatformIO host unit tests.
# Append: pioarduino passes its own EXTRA_COMPONENT_DIRS on the command line.
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/lib/brush_core")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(toothbrush)
