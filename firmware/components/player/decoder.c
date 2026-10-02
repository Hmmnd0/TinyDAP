#include "tinydap/decoder.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "third_party/dr_flac.h"
#include "tinydap/wav.h"

typedef enum { DEC_WAV, DEC_FLAC } dec_kind_t;

struct decoder {
    dec_kind_t kind;
    uint8_t channels;
    /* WAV */
    FILE *file;
    uint8_t bytes_per_sample;
    uint32_t frames_left;
    uint8_t in[DECODER_MAX_FRAMES * 2 * 3];
    /* FLAC */
    drflac *flac;
};

static const char *extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot : "";
}

int decoder_supports(const char *path)
{
    const char *ext = extension(path);
    return strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".flac") == 0;
}

static bool supported_format(const audio_format_t *f, const char **err)
{
    if (f->channels < 1 || f->channels > 2) {
        *err = "Need mono or stereo";
    } else if (f->bits_per_sample != 16 && f->bits_per_sample != 24) {
        *err = "Need 16/24-bit";
    } else if (f->sample_rate < 8000 || f->sample_rate > 96000) {
        *err = "Unsupported rate";
    } else {
        return true;
    }
    return false;
}

/* ---------- WAV ---------- */

static decoder_t *wav_open_dec(const char *path, decoder_info_t *info, const char **err)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        *err = "Can't open file";
        return NULL;
    }
    wav_info_t w;
    decoder_t *d = NULL;
    if (wav_open(f, &w, err) == 0 && supported_format(&w.fmt, err)) {
        d = calloc(1, sizeof *d);
        if (!d) {
            *err = "Out of memory";
        }
    }
    if (!d) {
        fclose(f);
        return NULL;
    }
    d->kind = DEC_WAV;
    d->file = f;
    d->channels = w.fmt.channels;
    d->bytes_per_sample = w.fmt.bits_per_sample / 8;
    d->frames_left = w.frames;
    info->fmt = w.fmt;
    info->total_frames = w.frames;
    info->codec = "WAV";
    return d;
}

static size_t wav_read(decoder_t *d, int16_t *out, size_t frames)
{
    const int bytes = d->bytes_per_sample;
    const size_t in_frame = (size_t)d->channels * bytes;
    if (frames > d->frames_left) {
        frames = d->frames_left;
    }
    size_t got = frames ? fread(d->in, 1, frames * in_frame, d->file) / in_frame : 0;
    d->frames_left -= (uint32_t)got;

    const int off = bytes - 2;  /* little-endian; for 24-bit keep the top 16 bits */
    for (size_t i = 0; i < got; i++) {
        const uint8_t *p = d->in + i * in_frame;
        int16_t l = (int16_t)(p[off] | p[off + 1] << 8);
        int16_t r = d->channels == 2 ? (int16_t)(p[bytes + off] | p[bytes + off + 1] << 8) : l;
        out[2 * i] = l;
        out[2 * i + 1] = r;
    }
    return got;
}

/* ---------- FLAC ---------- */

static void copy_tag(char *dst, size_t n, const char *val, size_t len)
{
    if (len >= n) {
        len = n - 1;
    }
    memcpy(dst, val, len);
    dst[len] = '\0';
}

static void flac_meta(void *user, drflac_metadata *m)
{
    if (m->type != DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
        return;
    }
    track_tags_t *tags = user;
    drflac_vorbis_comment_iterator it;
    drflac_init_vorbis_comment_iterator(&it, m->data.vorbis_comment.commentCount,
                                        m->data.vorbis_comment.pComments);
    const char *c;
    drflac_uint32 len;
    while ((c = drflac_next_vorbis_comment(&it, &len)) != NULL) {
        static const struct { const char *key; size_t off; } fields[] = {
            { "TITLE=", offsetof(track_tags_t, title) },
            { "ARTIST=", offsetof(track_tags_t, artist) },
            { "ALBUM=", offsetof(track_tags_t, album) },
        };
        for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
            size_t klen = strlen(fields[i].key);
            if (len > klen && strncasecmp(c, fields[i].key, klen) == 0) {
                copy_tag((char *)tags + fields[i].off, sizeof tags->title, c + klen, len - klen);
            }
        }
    }
}

static decoder_t *flac_open_dec(const char *path, decoder_info_t *info, const char **err)
{
    drflac *flac = drflac_open_file_with_metadata(path, flac_meta, &info->tags, NULL);
    if (!flac) {
        *err = "Can't decode FLAC";
        return NULL;
    }
    audio_format_t fmt = { .sample_rate = flac->sampleRate, .bits_per_sample = flac->bitsPerSample,
                           .channels = flac->channels };
    decoder_t *d = NULL;
    if (supported_format(&fmt, err)) {
        d = calloc(1, sizeof *d);
        if (!d) {
            *err = "Out of memory";
        }
    }
    if (!d) {
        drflac_close(flac);
        return NULL;
    }
    d->kind = DEC_FLAC;
    d->flac = flac;
    d->channels = flac->channels;
    info->fmt = fmt;
    info->total_frames = (uint32_t)flac->totalPCMFrameCount;
    info->codec = "FLAC";
    return d;
}

static size_t flac_read(decoder_t *d, int16_t *out, size_t frames)
{
    size_t got = (size_t)drflac_read_pcm_frames_s16(d->flac, frames, out);
    if (d->channels == 1) {
        /* Expand mono in place, back to front. */
        for (size_t i = got; i-- > 0;) {
            out[2 * i] = out[2 * i + 1] = out[i];
        }
    }
    return got;
}

/* ---------- dispatch ---------- */

decoder_t *decoder_open(const char *path, decoder_info_t *info, const char **err)
{
    const char *dummy;
    if (!err) {
        err = &dummy;
    }
    memset(info, 0, sizeof *info);
    const char *ext = extension(path);
    if (strcasecmp(ext, ".wav") == 0) {
        return wav_open_dec(path, info, err);
    }
    if (strcasecmp(ext, ".flac") == 0) {
        return flac_open_dec(path, info, err);
    }
    *err = "Format not supported";
    return NULL;
}

size_t decoder_read(decoder_t *d, int16_t *out, size_t frames)
{
    if (frames > DECODER_MAX_FRAMES) {
        frames = DECODER_MAX_FRAMES;
    }
    return d->kind == DEC_FLAC ? flac_read(d, out, frames) : wav_read(d, out, frames);
}

void decoder_close(decoder_t *d)
{
    if (!d) {
        return;
    }
    if (d->kind == DEC_FLAC) {
        drflac_close(d->flac);
    } else {
        fclose(d->file);
    }
    free(d);
}
