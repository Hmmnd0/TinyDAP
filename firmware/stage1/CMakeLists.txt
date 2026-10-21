# Stage 1 bring-up firmware for the ESP32-S3-DevKitC-1 breadboard
# (docs/Stage1_Wiring.md): SSD1306 OLED, five buttons, PCM5102A test tone.
# Reuses the portable components from ../components.
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/../components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tinydap_stage1)
