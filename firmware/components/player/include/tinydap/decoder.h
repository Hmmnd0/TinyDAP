#pragma once

#include <stddef.h>
#include <stdint.h>

#include "tinydap/audio_sink.h"

/*
 * Audio file decoders behind one interface. Output is always interleaved
 * 16-bit stereo at the source sample rate (mono is duplicated, 24-bit is
 * truncated to 16 for now). The decoder is picked from the file extension.
 */
typedef struct {
    char title[64];
    char artist[64];
    char album[64];
} track_tags_t;

typedef struct {
    audio_format_t fmt;      /* source format */
    uint32_t total_frames;   /* MP3 without a Xing header: CBR estimate */
    const char *codec;       /* "WAV", "FLAC", "MP3" */
    track_tags_t tags;       /* empty strings when absent */
} decoder_info_t;

typedef struct decoder decoder_t;

/* Max frames per decoder_read call. */
#define DECODER_MAX_FRAMES 1024

/* True if the extension is one we can decode. */
int decoder_supports(const char *path);

/* Returns NULL on failure with *err set to a short reason. */
decoder_t *decoder_open(const char *path, decoder_info_t *info, const char **err);

/* Decodes up to `frames` (<= DECODER_MAX_FRAMES). Returns frames written;
 * 0 means end of stream or error. */
size_t decoder_read(decoder_t *d, int16_t *out, size_t frames);

/* Cumulative time spent reading the file (storage), for load measurement. */
uint64_t decoder_read_time_us(const decoder_t *d);

void decoder_close(decoder_t *d);
