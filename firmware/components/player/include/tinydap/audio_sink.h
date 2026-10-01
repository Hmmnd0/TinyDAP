#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * Audio output abstraction (write-up §23 Phase 5: "audio output abstraction").
 *
 * The player pushes interleaved PCM into a sink without knowing what is
 * behind it: ES8311 on the Cardputer-Adv, PCM5102A on the breadboard,
 * CS43131 on Rev A, a WAV file on the host, or later a Bluetooth SoC.
 */
typedef struct {
    uint32_t sample_rate;
    uint8_t bits_per_sample;
    uint8_t channels;
} audio_format_t;

static inline size_t audio_format_frame_bytes(const audio_format_t *f)
{
    return (size_t)f->channels * (f->bits_per_sample / 8);
}

typedef struct audio_sink audio_sink_t;

struct audio_sink {
    /* Configure for a format. Returns 0 on success. May be called again on a
     * sample-rate change. */
    int (*open)(audio_sink_t *self, const audio_format_t *fmt);

    /* Write up to len bytes, blocking at most timeout_ms. Returns bytes
     * accepted, or a negative value on error. */
    int (*write)(audio_sink_t *self, const void *data, size_t len, uint32_t timeout_ms);

    void (*close)(audio_sink_t *self);

    void *ctx;  /* implementation state */
};
