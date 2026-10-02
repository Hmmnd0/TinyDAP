#pragma once

#include <stdbool.h>

#include "tinydap/audio_sink.h"
#include "tinydap/player_status.h"

/*
 * WAV playback engine (write-up §18): a decoder task reads the file and fills
 * the PCM ring; a higher-priority audio task drains it into the sink. Output
 * is always 16-bit stereo at the file's sample rate.
 *
 * `sink` must already be open at 44.1 kHz / 16-bit / stereo.
 */
void player_start(audio_sink_t *sink, bool mono_downmix);

void player_play(const char *path);
void player_toggle_pause(void);
void player_stop(void);
void player_get_status(player_status_t *out);
