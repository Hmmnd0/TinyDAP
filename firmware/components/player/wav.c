#include "tinydap/wav.h"

#include <stdbool.h>
#include <string.h>

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)le16(p) | (uint32_t)le16(p + 2) << 16; }

int wav_open(FILE *f, wav_info_t *info, const char **err)
{
    const char *dummy;
    if (!err) {
        err = &dummy;
    }
    uint8_t hdr[12];
    if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr ||
        memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        *err = "not a WAV file";
        return -1;
    }

    bool have_fmt = false;
    for (;;) {
        uint8_t ch[8];
        if (fread(ch, 1, sizeof ch, f) != sizeof ch) {
            *err = have_fmt ? "no data chunk" : "no fmt chunk";
            return -1;
        }
        uint32_t size = le32(ch + 4);

        if (memcmp(ch, "fmt ", 4) == 0) {
            uint8_t fmt[40];
            if (size < 16 || size > sizeof fmt || fread(fmt, 1, size, f) != size) {
                *err = "bad fmt chunk";
                return -1;
            }
            uint16_t tag = le16(fmt);
            if (tag == 0xFFFE && size >= 26) {
                tag = le16(fmt + 24);  /* extensible: subformat GUID starts with the tag */
            }
            if (tag != 1) {
                *err = "not integer PCM";
                return -1;
            }
            info->fmt.channels = (uint8_t)le16(fmt + 2);
            info->fmt.sample_rate = le32(fmt + 4);
            info->fmt.bits_per_sample = (uint8_t)le16(fmt + 14);
            have_fmt = true;
        } else if (memcmp(ch, "data", 4) == 0) {
            if (!have_fmt) {
                *err = "data before fmt";
                return -1;
            }
            size_t frame = audio_format_frame_bytes(&info->fmt);
            if (frame == 0) {
                *err = "bad format";
                return -1;
            }
            info->data_bytes = size;
            info->frames = size / (uint32_t)frame;
            return 0;
        } else if (fseek(f, (long)(size + (size & 1)), SEEK_CUR) != 0) {
            *err = "truncated";
            return -1;
        }
        if (memcmp(ch, "fmt ", 4) == 0 && (size & 1)) {
            fseek(f, 1, SEEK_CUR);
        }
    }
}
