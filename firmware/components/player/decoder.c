#include "tinydap/decoder.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "third_party/dr_flac.h"
#include "tinydap/wav.h"

typedef enum { DEC_WAV, DEC_FLAC } dec_kind_t;

/*
 * FLAC file reads go through a heap read-ahead buffer so each storage read
 * is one large multi-sector transfer, while dr_flac keeps its default 4 KB
 * cache. (Raising DR_FLAC_BUFFER_SIZE instead overflows the decoder stack:
 * dr_flac builds that cache on the stack while opening a file.)
 */
#define READ_AHEAD_BYTES (16 * 1024)

struct decoder {
    dec_kind_t kind;
    uint8_t channels;
    uint64_t read_us;           /* time spent reading the file */
    /* WAV */
    FILE *file;
    uint8_t bytes_per_sample;
    uint32_t frames_left;
    /* FLAC */
    drflac *flac;
    int fd;
    size_t rpos, rlen;          /* unread window of rbuf */
    track_tags_t *tags_out;     /* only valid during open */
    union {
        uint8_t in[DECODER_MAX_FRAMES * 2 * 3];  /* WAV conversion input */
        uint8_t rbuf[READ_AHEAD_BYTES];          /* FLAC read-ahead */
    };
};

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

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
    uint64_t t0 = now_us();
    size_t got = frames ? fread(d->in, 1, frames * in_frame, d->file) / in_frame : 0;
    d->read_us += now_us() - t0;
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

static size_t flac_on_read(void *user, void *buf, size_t n)
{
    decoder_t *d = user;
    uint8_t *out = buf;
    size_t done = 0;
    uint64_t t0 = now_us();

    while (done < n) {
        if (d->rpos == d->rlen) {
            if (n - done >= READ_AHEAD_BYTES) {
                /* Large request: read straight into the caller's buffer. */
                ssize_t r = read(d->fd, out + done, n - done);
                if (r <= 0) {
                    break;
                }
                done += (size_t)r;
                continue;
            }
            ssize_t r = read(d->fd, d->rbuf, READ_AHEAD_BYTES);
            if (r <= 0) {
                break;
            }
            d->rpos = 0;
            d->rlen = (size_t)r;
        }
        size_t take = d->rlen - d->rpos;
        if (take > n - done) {
            take = n - done;
        }
        memcpy(out + done, d->rbuf + d->rpos, take);
        d->rpos += take;
        done += take;
    }
    d->read_us += now_us() - t0;
    return done;
}

/* Logical stream position: file position minus what's still buffered. */
static off_t flac_logical_pos(decoder_t *d)
{
    off_t pos = lseek(d->fd, 0, SEEK_CUR);
    return pos < 0 ? pos : pos - (off_t)(d->rlen - d->rpos);
}

static drflac_bool32 flac_on_seek(void *user, int offset, drflac_seek_origin origin)
{
    decoder_t *d = user;
    if (origin == DRFLAC_SEEK_CUR) {
        /* Stay inside the buffer when possible. */
        long np = (long)d->rpos + offset;
        if (np >= 0 && np <= (long)d->rlen) {
            d->rpos = (size_t)np;
            return DRFLAC_TRUE;
        }
        off_t target = flac_logical_pos(d) + offset;
        d->rpos = d->rlen = 0;
        return target >= 0 && lseek(d->fd, target, SEEK_SET) >= 0;
    }
    d->rpos = d->rlen = 0;
    return lseek(d->fd, offset, origin == DRFLAC_SEEK_SET ? SEEK_SET : SEEK_END) >= 0;
}

static drflac_bool32 flac_on_tell(void *user, drflac_int64 *cursor)
{
    off_t pos = flac_logical_pos(user);
    if (pos < 0) {
        return DRFLAC_FALSE;
    }
    *cursor = pos;
    return DRFLAC_TRUE;
}

static void flac_meta(void *user, drflac_metadata *m)
{
    if (m->type != DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
        return;
    }
    track_tags_t *tags = ((decoder_t *)user)->tags_out;
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
    decoder_t *d = calloc(1, sizeof *d);
    if (!d) {
        *err = "Out of memory";
        return NULL;
    }
    d->fd = open(path, O_RDONLY);
    if (d->fd < 0) {
        *err = "Can't open file";
        free(d);
        return NULL;
    }
    d->tags_out = &info->tags;
    drflac *flac = drflac_open_with_metadata(flac_on_read, flac_on_seek, flac_on_tell, flac_meta, d, NULL);
    d->tags_out = NULL;
    if (!flac) {
        *err = "Can't decode FLAC";
        close(d->fd);
        free(d);
        return NULL;
    }
    audio_format_t fmt = { .sample_rate = flac->sampleRate, .bits_per_sample = flac->bitsPerSample,
                           .channels = flac->channels };
    if (!supported_format(&fmt, err)) {
        drflac_close(flac);
        close(d->fd);
        free(d);
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

uint64_t decoder_read_time_us(const decoder_t *d)
{
    return d ? d->read_us : 0;
}

void decoder_close(decoder_t *d)
{
    if (!d) {
        return;
    }
    if (d->kind == DEC_FLAC) {
        drflac_close(d->flac);
        close(d->fd);
    } else {
        fclose(d->file);
    }
    free(d);
}
