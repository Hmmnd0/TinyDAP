#pragma once

#include <stddef.h>
#include <stdint.h>

/* Sine test-tone generator. Stands in for the decoder until dr_flac lands,
 * and stays useful afterwards for testing new audio sinks. */
typedef struct {
    float phase;  /* radians, [0, 2π) */
    float step;   /* radians per frame */
    float amp;    /* 0.0 .. 1.0 of full scale */
} tone_t;

void tone_init(tone_t *t, uint32_t sample_rate, float freq_hz, float amplitude);

/* Fills `frames` interleaved 16-bit stereo frames (L = R). */
void tone_fill_s16_stereo(tone_t *t, int16_t *dst, size_t frames);
