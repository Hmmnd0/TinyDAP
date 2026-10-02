/*
 * Decodes an audio file with TinyDAP's decoders and writes 16-bit stereo
 * WAV, for checking decoder output against a reference.
 *
 * Usage: decode_to_wav <in.flac|in.wav> <out.wav>
 */

#include <stdio.h>

#include "tinydap/decoder.h"
#include "wav_sink.h"

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <in> <out.wav>\n", argv[0]);
        return 1;
    }
    decoder_info_t info;
    const char *err;
    decoder_t *d = decoder_open(argv[1], &info, &err);
    if (!d) {
        fprintf(stderr, "%s: %s\n", argv[1], err);
        return 1;
    }
    audio_sink_t *sink = wav_sink_create(argv[2]);
    audio_format_t out = { .sample_rate = info.fmt.sample_rate, .bits_per_sample = 16, .channels = 2 };
    if (!sink || sink->open(sink, &out) != 0) {
        fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }

    static int16_t buf[DECODER_MAX_FRAMES * 2];
    size_t n, total = 0;
    while ((n = decoder_read(d, buf, DECODER_MAX_FRAMES)) > 0) {
        sink->write(sink, buf, n * 4, 0);
        total += n;
    }
    sink->close(sink);
    wav_sink_destroy(sink);
    decoder_close(d);

    printf("%s %u Hz %u-bit %u ch, %zu/%u frames | %s / %s / %s\n", info.codec,
           (unsigned)info.fmt.sample_rate, info.fmt.bits_per_sample, info.fmt.channels, total,
           (unsigned)info.total_frames, info.tags.artist, info.tags.album, info.tags.title);
    return total == info.total_frames ? 0 : 2;
}
