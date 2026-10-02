#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

/*
 * Minimal ES8311 codec driver: DAC playback only, codec as I2S slave,
 * internal clock derived from BCLK (no MCLK pin). Expects 16-bit stereo
 * Philips I2S at BCLK = 32 x fs. The ES8311 is mono; it plays the left slot.
 *
 * BCLK must already be running (I2S channel enabled) before es8311_init().
 */
esp_err_t es8311_init(i2c_master_bus_handle_t bus, uint8_t addr);

/* DAC digital volume in dB, clamped to -95..0. */
esp_err_t es8311_set_volume_db(int db);
