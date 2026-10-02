#pragma once

#include "tinydap/audio_sink.h"

/* audio_sink backed by an ESP32-S3 I2S peripheral in standard (Philips)
 * mode, TX only, ESP32 as clock master. Writes block on the DMA queue,
 * which paces the caller at the playback rate. */
audio_sink_t *i2s_sink_create(int port, int bclk, int ws, int dout, int mclk);
