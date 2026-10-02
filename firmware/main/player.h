#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tinydap/audio_sink.h"
#include "tinydap/player_status.h"

/*
 * Playback engine (write-up §18): a decoder task decodes WAV/FLAC and fills
 * the PCM ring; a higher-priority audio task drains it into the sink. Output
 * is always 16-bit stereo at the file's sample rate.
 *
 * `sink` must already be open at 44.1 kHz / 16-bit / stereo.
 */
void player_start(audio_sink_t *sink, bool mono_downmix);

void player_play(const char *path);
void player_toggle_pause(void);
void player_stop(void);
void player_set_next(const char *path);   /* NULL clears */
void player_get_status(player_status_t *out);

/* Cumulative decoder load counters; compare two snapshots. */
typedef struct {
    uint32_t busy_us;   /* time spent decoding, including storage reads */
    uint32_t read_us;   /* time spent reading storage */
    uint32_t frames;    /* frames decoded */
    uint32_t read_bytes;
    uint32_t read_calls;
} player_perf_t;

void player_get_perf(player_perf_t *out);
