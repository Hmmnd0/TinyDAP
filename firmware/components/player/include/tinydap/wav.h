#pragma once

#include <stdint.h>
#include <stdio.h>

#include "tinydap/audio_sink.h"

typedef struct {
    audio_format_t fmt;
    uint32_t data_bytes;   /* size of the PCM data chunk */
    uint32_t frames;       /* data_bytes / frame size */
} wav_info_t;

/*
 * Parses a RIFF/WAVE header and leaves `f` positioned at the first PCM byte.
 * Accepts integer PCM (format 1, or WAVE_FORMAT_EXTENSIBLE with PCM subtype).
 * Returns 0 on success or a negative value; *err gets a short reason.
 */
int wav_open(FILE *f, wav_info_t *info, const char **err);
