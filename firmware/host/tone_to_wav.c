/*
 * Runs the Stage 0 pipeline shape on the host: a producer thread (decoder
 * stand-in) fills the PCM ring with a test tone while a consumer thread
 * (audio output) drains it into a WAV sink.
 *
 * Usage: tone_to_wav [out.wav]
 */

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "tinydap/pcm_ring.h"
#include "tinydap/tone.h"
#include "wav_sink.h"

#define SAMPLE_RATE  44100
#define SECONDS      3
#define RING_BYTES   (16 * 1024)
#define CHUNK_FRAMES 256

static pcm_ring_t s_ring;
static atomic_bool s_done;

static void *producer(void *arg)
{
    (void)arg;
    tone_t tone;
    tone_init(&tone, SAMPLE_RATE, 440.0f, 0.25f);
    int16_t chunk[CHUNK_FRAMES * 2];

    for (size_t frames = 0; frames < (size_t)SAMPLE_RATE * SECONDS; frames += CHUNK_FRAMES) {
        tone_fill_s16_stereo(&tone, chunk, CHUNK_FRAMES);
        const uint8_t *p = (const uint8_t *)chunk;
        size_t left = sizeof chunk;
        while (left > 0) {
            size_t n = pcm_ring_write(&s_ring, p, left);
            p += n;
            left -= n;
            if (left > 0) {
                usleep(100);
            }
        }
    }
    atomic_store(&s_done, true);
    return NULL;
}

static void *consumer(void *arg)
{
    audio_sink_t *sink = arg;
    uint8_t buf[1024];

    for (;;) {
        size_t n = pcm_ring_read(&s_ring, buf, sizeof buf);
        if (n > 0) {
            sink->write(sink, buf, n, 0);
        } else if (atomic_load(&s_done) && pcm_ring_used(&s_ring) == 0) {
            break;
        } else {
            usleep(100);
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tone.wav";
    static uint8_t ring_mem[RING_BYTES];
    pcm_ring_init(&s_ring, ring_mem, sizeof ring_mem);

    audio_sink_t *sink = wav_sink_create(path);
    audio_format_t fmt = { .sample_rate = SAMPLE_RATE, .bits_per_sample = 16, .channels = 2 };
    if (!sink || sink->open(sink, &fmt) != 0) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, sink);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    sink->close(sink);
    wav_sink_destroy(sink);
    printf("wrote %d s of 440 Hz to %s\n", SECONDS, path);
    return 0;
}
