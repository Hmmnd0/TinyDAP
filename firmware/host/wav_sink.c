#include "wav_sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    audio_sink_t base;
    const char *path;
    FILE *f;
    audio_format_t fmt;
    uint32_t data_bytes;
} wav_sink_t;

static void put_u16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put_u32(uint8_t *p, uint32_t v) { put_u16(p, v); put_u16(p + 2, v >> 16); }

static void write_header(wav_sink_t *s)
{
    uint16_t block_align = (uint16_t)audio_format_frame_bytes(&s->fmt);
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    put_u32(h + 4, 36 + s->data_bytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    put_u32(h + 16, 16);
    put_u16(h + 20, 1);  /* PCM */
    put_u16(h + 22, s->fmt.channels);
    put_u32(h + 24, s->fmt.sample_rate);
    put_u32(h + 28, s->fmt.sample_rate * block_align);
    put_u16(h + 32, block_align);
    put_u16(h + 34, s->fmt.bits_per_sample);
    memcpy(h + 36, "data", 4);
    put_u32(h + 40, s->data_bytes);

    fseek(s->f, 0, SEEK_SET);
    fwrite(h, 1, sizeof h, s->f);
    fseek(s->f, 0, SEEK_END);
}

static int wav_open(audio_sink_t *self, const audio_format_t *fmt)
{
    wav_sink_t *s = self->ctx;
    s->f = fopen(s->path, "wb");
    if (!s->f) {
        return -1;
    }
    s->fmt = *fmt;
    s->data_bytes = 0;
    write_header(s);
    return 0;
}

static int wav_write(audio_sink_t *self, const void *data, size_t len, uint32_t timeout_ms)
{
    (void)timeout_ms;
    wav_sink_t *s = self->ctx;
    size_t n = fwrite(data, 1, len, s->f);
    s->data_bytes += (uint32_t)n;
    return (int)n;
}

static void wav_close(audio_sink_t *self)
{
    wav_sink_t *s = self->ctx;
    if (s->f) {
        write_header(s);  /* patch in the final sizes */
        fclose(s->f);
        s->f = NULL;
    }
}

audio_sink_t *wav_sink_create(const char *path)
{
    wav_sink_t *s = calloc(1, sizeof *s);
    if (!s) {
        return NULL;
    }
    s->path = path;
    s->base.open = wav_open;
    s->base.write = wav_write;
    s->base.close = wav_close;
    s->base.ctx = s;
    return &s->base;
}

void wav_sink_destroy(audio_sink_t *sink)
{
    free(sink->ctx);
}
