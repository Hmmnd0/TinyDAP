#pragma once

/* ESP32-S3-DevKitC-1 breadboard pin map. Source: docs/Stage1_Wiring.md. */

/* PCM5102A, I2S standard mode, no MCLK (the DAC's PLL makes its own) */
#define BOARD_I2S_PORT  0
#define BOARD_I2S_BCLK  5
#define BOARD_I2S_WS    6
#define BOARD_I2S_DOUT  7

/* SSD1306 128x64 OLED, 4-wire SPI */
#define BOARD_OLED_SPI_HOST SPI2_HOST
#define BOARD_OLED_SCK  12
#define BOARD_OLED_MOSI 11
#define BOARD_OLED_CS   10
#define BOARD_OLED_DC   9
#define BOARD_OLED_RES  13

/* Buttons to GND, internal pull-ups */
#define BOARD_BTN_PLAY  4
#define BOARD_BTN_PREV  15
#define BOARD_BTN_NEXT  16
#define BOARD_BTN_VOLUP 17
#define BOARD_BTN_VOLDN 18

/* microSD, 4-bit SDIO (not used by the bring-up firmware yet) */
#define BOARD_SD_CLK  39
#define BOARD_SD_CMD  40
#define BOARD_SD_D0   41
#define BOARD_SD_D1   42
#define BOARD_SD_D2   2
#define BOARD_SD_D3   1
#define BOARD_SD_DET  21
