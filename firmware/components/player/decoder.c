#include "tinydap/decoder.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "third_party/dr_flac.h"
#include "third_party/minimp3.h"
#include "tinydap/wav.h"

typedef enum { DEC_WAV, DEC_FLAC, DEC_MP3 } dec_kind_t;

/*
 * FLAC file reads go through a heap read-ahead buffer so each storage read
 * is one large multi-sector transfer, while dr_flac keeps its default 4 KB
 * cache. (Raising DR_FLAC_BUFFER_SIZE instead overflows the decoder stack:
 * dr_flac builds that cache on the stack while opening a file.)
 */
#define READ_AHEAD_BYTES (16 * 1024)

/*
 * Storage reads are kept aligned: 4-byte destination, whole 512-byte
 * sectors, from a sector-aligned file position. ESP-IDF's SD-over-SPI
 * driver reads straight into the buffer only when address and size are
 * multiples of 4; otherwise it allocates a temporary DMA buffer on every
 * read and copies (measured on device: MP3 refills ran ~5x slower per byte
 * than aligned FLAC reads). Misaligned file positions also force FatFs
 * through its single-sector window.
 */
#define SECTOR 512

/* MP3: refill the input window when less than this is buffered (several
 * maximum-size frames), so the decoder always sees whole frames. */
#define MP3_REFILL_BELOW 4096

typedef struct {
    mp3dec_t dec;
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t pcm_pos, pcm_len;    /* frames of the current decoded block */
    int channels;               /* of the current block */
    size_t in_pos, in_len;      /* undecoded window of rbuf */
    bool eof;
} mp3_state_t;

struct decoder {
    dec_kind_t kind;
    uint8_t channels;
    uint64_t read_us;           /* time spent reading the file */
    uint64_t read_bytes;
    uint32_t read_calls;
    /* WAV */
    FILE *file;
    uint8_t bytes_per_sample;
    uint32_t frames_left;
    /* FLAC and MP3 */
    drflac *flac;
    mp3_state_t *mp3;
    int fd;
    size_t rpos, rlen;          /* unread window of rbuf */
    track_tags_t *tags_out;     /* only valid during open */
    union {
        uint8_t in[DECODER_MAX_FRAMES * 2 * 3];  /* WAV conversion input */
        _Alignas(4) uint8_t rbuf[READ_AHEAD_BYTES];  /* FLAC read-ahead / MP3 input */
    };
};

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static ssize_t timed_read(decoder_t *d, void *buf, size_t n)
{
    uint64_t t0 = now_us();
    ssize_t r = read(d->fd, buf, n);
    d->read_us += now_us() - t0;
    d->read_calls++;
    if (r > 0) {
        d->read_bytes += (uint64_t)r;
    }
    if (r < 0) {
        fprintf(stderr, "decoder: read error %d (%s)\n", errno, strerror(errno));
    }
    return r;
}

/* Bytes to read next so the file position lands on a sector boundary, or
 * `space` rounded down to whole sectors if it already is. */
static size_t aligned_read_size(decoder_t *d, size_t space)
{
    off_t pos = lseek(d->fd, 0, SEEK_CUR);
    size_t head = pos < 0 ? 0 : (size_t)(pos % SECTOR);
    if (head) {
        size_t n = SECTOR - head;
        return n < space ? n : space;
    }
    return space / SECTOR * SECTOR;
}

static const char *extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    return dot ? dot : "";
}

int decoder_supports(const char *path)
{
    const char *ext = extension(path);
    return strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".flac") == 0 ||
           strcasecmp(ext, ".mp3") == 0;
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
    d->read_calls++;
    d->read_bytes += got * in_frame;
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

    while (done < n) {
        if (d->rpos == d->rlen) {
            ssize_t r = timed_read(d, d->rbuf, aligned_read_size(d, READ_AHEAD_BYTES));
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

/* ---------- MP3 ---------- */

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

static uint32_t syncsafe32(const uint8_t *p)
{
    return (uint32_t)(p[0] & 0x7F) << 21 | (uint32_t)(p[1] & 0x7F) << 14 |
           (uint32_t)(p[2] & 0x7F) << 7 | (p[3] & 0x7F);
}

/* ID3v2 text frame -> ASCII ('?' for characters the 5x7 font can't show). */
static void id3_text(char *dst, size_t n, const uint8_t *p, size_t len)
{
    size_t o = 0;
    if (len > 0) {
        uint8_t enc = p[0];
        p++;
        len--;
        if (enc == 1 || enc == 2) {  /* UTF-16 with BOM / UTF-16BE */
            bool be = enc == 2;
            if (len >= 2 && p[0] == 0xFF && p[1] == 0xFE) {
                be = false;
                p += 2;
                len -= 2;
            } else if (len >= 2 && p[0] == 0xFE && p[1] == 0xFF) {
                be = true;
                p += 2;
                len -= 2;
            }
            for (size_t i = 0; i + 1 < len && o < n - 1; i += 2) {
                unsigned c = be ? (unsigned)(p[i] << 8 | p[i + 1]) : (unsigned)(p[i] | p[i + 1] << 8);
                if (c == 0) {
                    break;
                }
                dst[o++] = c < 0x80 ? (char)c : '?';
            }
        } else {  /* 0 = ISO-8859-1, 3 = UTF-8 */
            for (size_t i = 0; i < len && o < n - 1; i++) {
                uint8_t c = p[i];
                if (c == 0) {
                    break;
                }
                if (c < 0x80) {
                    dst[o++] = (char)c;
                } else if (enc != 3 || (c & 0xC0) != 0x80) {
                    dst[o++] = '?';  /* UTF-8 continuation bytes are dropped */
                }
            }
        }
    }
    dst[o] = '\0';
}

/* Reads title/artist/album from an ID3v2.3/2.4 tag and leaves the file at
 * the first byte after the tag (or at 0 if there is none). */
static void mp3_read_id3(decoder_t *d, track_tags_t *tags)
{
    uint8_t h[10];
    if (read(d->fd, h, sizeof h) != (ssize_t)sizeof h || memcmp(h, "ID3", 3) != 0) {
        lseek(d->fd, 0, SEEK_SET);
        return;
    }
    const int ver = h[3];
    const uint32_t body_end = 10 + syncsafe32(h + 6);
    const off_t tag_end = (off_t)body_end + ((h[5] & 0x10) ? 10 : 0);

    /* Skip frame parsing for unsynchronised or extended-header tags. */
    if ((ver == 3 || ver == 4) && !(h[5] & 0xC0)) {
        off_t pos = 10;
        uint8_t fh[10];
        while (pos + 10 <= (off_t)body_end && read(d->fd, fh, sizeof fh) == (ssize_t)sizeof fh &&
               fh[0] != 0) {
            uint32_t size = ver == 4 ? syncsafe32(fh + 4) : be32(fh + 4);
            pos += 10;
            char *dst = memcmp(fh, "TIT2", 4) == 0 ? tags->title
                      : memcmp(fh, "TPE1", 4) == 0 ? tags->artist
                      : memcmp(fh, "TALB", 4) == 0 ? tags->album : NULL;
            uint8_t text[256];
            if (dst && size > 1 && size <= sizeof text &&
                read(d->fd, text, size) == (ssize_t)size) {
                id3_text(dst, sizeof tags->title, text, size);
            }
            pos += size;
            lseek(d->fd, pos, SEEK_SET);
        }
    }
    lseek(d->fd, tag_end, SEEK_SET);
}

/* Compacts and refills the MP3 input window, keeping reads aligned: the
 * unread bytes are shifted so new data lands on a 4-byte boundary. */
static void mp3_fill(decoder_t *d)
{
    mp3_state_t *m = d->mp3;
    if (m->eof) {
        return;
    }
    size_t rem = m->in_len - m->in_pos;
    size_t start = (4 - rem % 4) % 4;
    if (m->in_pos != start) {
        memmove(d->rbuf + start, d->rbuf + m->in_pos, rem);
        m->in_pos = start;
        m->in_len = start + rem;
    }
    size_t want = aligned_read_size(d, READ_AHEAD_BYTES - m->in_len);
    if (want == 0) {
        return;
    }
    ssize_t r = timed_read(d, d->rbuf + m->in_len, want);
    if (r <= 0) {
        m->eof = true;
    } else {
        m->in_len += (size_t)r;
    }
}

/* Decodes the next audio frame into m->pcm. Returns samples per channel,
 * 0 at end of stream. *frame points at the frame's bytes (valid until the
 * next call). */
static int mp3_next_frame(decoder_t *d, mp3dec_frame_info_t *fi, const uint8_t **frame)
{
    mp3_state_t *m = d->mp3;
    for (;;) {
        if (m->in_len - m->in_pos < MP3_REFILL_BELOW) {
            mp3_fill(d);
        }
        size_t avail = m->in_len - m->in_pos;
        if (avail == 0) {
            return 0;
        }
        int samples = mp3dec_decode_frame(&m->dec, d->rbuf + m->in_pos, (int)avail, m->pcm, fi);
        if (fi->frame_bytes == 0) {
            if (m->eof) {
                return 0;
            }
            /* Not enough data for a frame yet: read more. If nothing more
             * fits, the window is full of non-MP3 data: skip it. */
            mp3_fill(d);
            if (m->in_len - m->in_pos == avail) {
                m->in_pos = m->in_len;
            }
            continue;
        }
        if (frame) {
            *frame = d->rbuf + m->in_pos + fi->frame_offset;
        }
        m->in_pos += (size_t)fi->frame_bytes;
        if (samples > 0) {
            return samples;
        }
    }
}

/* Frame count from a Xing/Info header in the first frame, if present. */
static bool mp3_xing_frames(const uint8_t *f, int len, uint32_t *frames)
{
    if (len < 4) {
        return false;
    }
    bool mpeg1 = ((f[1] >> 3) & 3) == 3;
    bool mono = (f[3] >> 6) == 3;
    int off = 4 + (mpeg1 ? (mono ? 17 : 32) : (mono ? 9 : 17));
    if (!(f[1] & 1)) {
        off += 2;  /* CRC present */
    }
    if (off + 12 > len || (memcmp(f + off, "Xing", 4) != 0 && memcmp(f + off, "Info", 4) != 0)) {
        return false;
    }
    if (!(be32(f + off + 4) & 1)) {
        return false;
    }
    *frames = be32(f + off + 8);
    return true;
}

static void mp3_free(decoder_t *d)
{
    if (d->fd >= 0) {
        close(d->fd);
    }
    free(d->mp3);
    free(d);
}

static decoder_t *mp3_open_dec(const char *path, decoder_info_t *info, const char **err)
{
    decoder_t *d = calloc(1, sizeof *d);
    if (!d || !(d->mp3 = calloc(1, sizeof *d->mp3))) {
        free(d);
        *err = "Out of memory";
        return NULL;
    }
    d->kind = DEC_MP3;
    d->fd = open(path, O_RDONLY);
    if (d->fd < 0) {
        *err = "Can't open file";
        mp3_free(d);
        return NULL;
    }
    off_t file_size = lseek(d->fd, 0, SEEK_END);
    lseek(d->fd, 0, SEEK_SET);
    mp3_read_id3(d, &info->tags);
    off_t audio_start = lseek(d->fd, 0, SEEK_CUR);

    mp3_state_t *m = d->mp3;
    mp3dec_init(&m->dec);
    mp3dec_frame_info_t fi;
    const uint8_t *frame = NULL;
    int n = mp3_next_frame(d, &fi, &frame);
    if (n <= 0) {
        *err = "Can't decode MP3";
        mp3_free(d);
        return NULL;
    }

    uint32_t xing = 0;
    if (mp3_xing_frames(frame, fi.frame_bytes, &xing)) {
        info->total_frames = xing * (uint32_t)n;
        m->pcm_len = 0;  /* the Xing/Info frame carries no audio */
    } else {
        m->pcm_len = (size_t)n;
        if (fi.bitrate_kbps > 0 && file_size > audio_start) {
            /* CBR estimate */
            info->total_frames = (uint32_t)((uint64_t)(file_size - audio_start) * 8 * fi.hz /
                                            ((uint64_t)fi.bitrate_kbps * 1000));
        }
    }
    m->pcm_pos = 0;
    m->channels = fi.channels;

    info->fmt.sample_rate = (uint32_t)fi.hz;
    info->fmt.bits_per_sample = 16;
    info->fmt.channels = (uint8_t)fi.channels;
    info->codec = "MP3";
    if (!supported_format(&info->fmt, err)) {
        mp3_free(d);
        return NULL;
    }
    return d;
}

static size_t mp3_read(decoder_t *d, int16_t *out, size_t frames)
{
    mp3_state_t *m = d->mp3;
    size_t done = 0;
    while (done < frames) {
        if (m->pcm_pos == m->pcm_len) {
            mp3dec_frame_info_t fi;
            int n = mp3_next_frame(d, &fi, NULL);
            if (n <= 0) {
                break;
            }
            m->pcm_pos = 0;
            m->pcm_len = (size_t)n;
            m->channels = fi.channels;
        }
        size_t take = m->pcm_len - m->pcm_pos;
        if (take > frames - done) {
            take = frames - done;
        }
        for (size_t i = 0; i < take; i++) {
            size_t src = m->pcm_pos + i;
            int16_t l = m->channels == 2 ? m->pcm[2 * src] : m->pcm[src];
            int16_t r = m->channels == 2 ? m->pcm[2 * src + 1] : l;
            out[2 * (done + i)] = l;
            out[2 * (done + i) + 1] = r;
        }
        m->pcm_pos += take;
        done += take;
    }
    return done;
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
    if (strcasecmp(ext, ".mp3") == 0) {
        return mp3_open_dec(path, info, err);
    }
    *err = "Format not supported";
    return NULL;
}

size_t decoder_read(decoder_t *d, int16_t *out, size_t frames)
{
    if (frames > DECODER_MAX_FRAMES) {
        frames = DECODER_MAX_FRAMES;
    }
    switch (d->kind) {
    case DEC_FLAC: return flac_read(d, out, frames);
    case DEC_MP3: return mp3_read(d, out, frames);
    default: return wav_read(d, out, frames);
    }
}

uint64_t decoder_read_time_us(const decoder_t *d)
{
    return d ? d->read_us : 0;
}

void decoder_read_stats(const decoder_t *d, uint64_t *bytes, uint32_t *calls)
{
    *bytes = d ? d->read_bytes : 0;
    *calls = d ? d->read_calls : 0;
}

void decoder_close(decoder_t *d)
{
    if (!d) {
        return;
    }
    if (d->kind == DEC_MP3) {
        mp3_free(d);
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
