#pragma once

/*
 * M5Stack Cardputer-Adv (K132-ADV) — Stage 0 development board.
 *
 * Sources:
 *   I2S codec pins   M5Unified board config (board_M5CardputerADV)
 *   microSD, ext I2S EMBER src/main.cpp (known working)
 *   I2C pins         M5Unified internal I2C table (SCL 9, SDA 8)
 *
 * Verify against the current M5Stack schematic before relying on anything
 * marked "unverified".
 */

#define BOARD_NAME "M5Stack Cardputer-Adv"

/* ES8311 codec: control over I2C */
#define BOARD_CODEC_I2C_SDA   8
#define BOARD_CODEC_I2C_SCL   9
#define BOARD_CODEC_I2C_ADDR  0x18  /* as used by M5Unified */

/* ES8311 codec: audio over I2S */
#define BOARD_CODEC_I2S_PORT  1
#define BOARD_CODEC_I2S_BCLK  41
#define BOARD_CODEC_I2S_WS    43
#define BOARD_CODEC_I2S_DOUT  42    /* ESP32 -> ES8311 DAC */
#define BOARD_CODEC_I2S_DIN   46    /* ES8311 ADC (mic) -> ESP32 */
#define BOARD_CODEC_I2S_MCLK  (-1)  /* not used by M5Unified; ES8311 clocks from BCLK */

/* Built-in microSD (SPI mode) */
#define BOARD_SD_SCK   40
#define BOARD_SD_MISO  39
#define BOARD_SD_MOSI  14
#define BOARD_SD_CS    12

/* External I2S DAC on the second I2S peripheral, as wired by EMBER.
 * The ES8311 is mono; this PCM5102A path is the Stage 0 stereo output and
 * early PCM5102A bring-up before Stage 1. Tie the PCM5102A SCK pin to GND. */
#define BOARD_EXT_I2S_PORT  0
#define BOARD_EXT_I2S_BCLK  5
#define BOARD_EXT_I2S_WS    6
#define BOARD_EXT_I2S_DOUT  3

/* ST7789 LCD, 240 x 135 landscape, own SPI bus (M5GFX board config) */
#define BOARD_LCD_SPI_HOST  SPI3_HOST
#define BOARD_LCD_MOSI      35
#define BOARD_LCD_SCLK      36
#define BOARD_LCD_DC        34
#define BOARD_LCD_CS        37
#define BOARD_LCD_RST       33
#define BOARD_LCD_BL        38
#define BOARD_LCD_W         240
#define BOARD_LCD_H         135
/* Landscape orientation: panel RAM offsets after swapping axes. If the
 * picture is mirrored or upside down, flip MIRROR_X/Y and use gap y 52. */
#define BOARD_LCD_SWAP_XY   1
#define BOARD_LCD_MIRROR_X  1
#define BOARD_LCD_MIRROR_Y  0
#define BOARD_LCD_GAP_X     40
#define BOARD_LCD_GAP_Y     53

/* microSD SPI host (the LCD uses SPI3) */
#define BOARD_SD_SPI_HOST   SPI2_HOST

/* TCA8418 keyboard matrix controller on the internal I2C bus (M5Cardputer) */
#define BOARD_KB_I2C_ADDR   0x34
#define BOARD_KB_INT        11
