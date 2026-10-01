#pragma once

#include "tinydap/audio_sink.h"

/* Host audio sink that writes a PCM WAV file. */
audio_sink_t *wav_sink_create(const char *path);
void wav_sink_destroy(audio_sink_t *sink);
