#pragma once

/*
 * M5Stack Cardputer-Adv (K132-ADV) — Stage 0 development board.
 *
 * Sources:
 *   I2S codec pins   M5Unified board config (board_M5CardputerADV)
 *   microSD, ext I2S EMBER src/main.cpp (known working)
 *   I2C pins         write-up §17 — not yet cross-checked
 *
 * Verify against the current M5Stack schematic before relying on anything
 * marked "unverified".
 */

#define BOARD_NAME "M5Stack Cardputer-Adv"

/* ES8311 codec: control over I2C */
#define BOARD_CODEC_I2C_SDA   8     /* unverified */
#define BOARD_CODEC_I2C_SCL   9     /* unverified */
#define BOARD_CODEC_I2C_ADDR  0x18  /* ES8311 default address; unverified */

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
