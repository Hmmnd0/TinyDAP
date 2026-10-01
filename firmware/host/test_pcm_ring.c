#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinydap/pcm_ring.h"

static int s_failures;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,      \
                    __LINE__, #cond);                                   \
            s_failures++;                                               \
        }                                                               \
    } while (0)

static void test_init_rejects_bad_capacity(void)
{
    pcm_ring_t r;
    uint8_t mem[16];
    CHECK(!pcm_ring_init(&r, mem, 0));
    CHECK(!pcm_ring_init(&r, mem, 12));
    CHECK(!pcm_ring_init(&r, NULL, 16));
    CHECK(pcm_ring_init(&r, mem, 16));
}

static void test_fill_and_drain(void)
{
    pcm_ring_t r;
    uint8_t mem[16];
    pcm_ring_init(&r, mem, sizeof mem);

    uint8_t in[20], out[20];
    for (int i = 0; i < 20; i++) {
        in[i] = (uint8_t)i;
    }

    CHECK(pcm_ring_used(&r) == 0);
    CHECK(pcm_ring_write(&r, in, 20) == 16);  /* partial: only 16 fit */
    CHECK(pcm_ring_free(&r) == 0);
    CHECK(pcm_ring_write(&r, in, 1) == 0);

    CHECK(pcm_ring_read(&r, out, 20) == 16);
    CHECK(memcmp(in, out, 16) == 0);
    CHECK(pcm_ring_used(&r) == 0);
    CHECK(pcm_ring_read(&r, out, 1) == 0);
}

static void test_wraparound(void)
{
    pcm_ring_t r;
    uint8_t mem[16];
    pcm_ring_init(&r, mem, sizeof mem);

    uint8_t in[10], out[10];
    for (int round = 0; round < 50; round++) {
        for (int i = 0; i < 10; i++) {
            in[i] = (uint8_t)(round * 10 + i);
        }
        CHECK(pcm_ring_write(&r, in, 10) == 10);
        CHECK(pcm_ring_read(&r, out, 10) == 10);
        CHECK(memcmp(in, out, 10) == 0);
    }
}

/* One producer thread and one consumer thread move a byte sequence through a
 * small ring in irregular chunk sizes; the consumer verifies every byte. */
#define STRESS_BYTES (20u * 1024 * 1024)

static pcm_ring_t s_stress;

static uint32_t lcg(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s >> 16;
}

static void *stress_producer(void *arg)
{
    (void)arg;
    uint8_t buf[1024];
    uint32_t seed = 1, next = 0;
    while (next < STRESS_BYTES) {
        size_t want = 1 + lcg(&seed) % sizeof buf;
        for (size_t i = 0; i < want; i++) {
            buf[i] = (uint8_t)(next + i);
        }
        next += (uint32_t)pcm_ring_write(&s_stress, buf, want);
    }
    return NULL;
}

static void *stress_consumer(void *arg)
{
    uint32_t *errors = arg;
    uint8_t buf[1024];
    uint32_t seed = 2, next = 0;
    while (next < STRESS_BYTES) {
        size_t n = pcm_ring_read(&s_stress, buf, 1 + lcg(&seed) % sizeof buf);
        for (size_t i = 0; i < n; i++) {
            if (buf[i] != (uint8_t)(next + i)) {
                (*errors)++;
            }
        }
        next += (uint32_t)n;
    }
    return NULL;
}

static void test_threaded_stress(void)
{
    static uint8_t mem[4096];
    pcm_ring_init(&s_stress, mem, sizeof mem);

    uint32_t errors = 0;
    pthread_t prod, cons;
    pthread_create(&prod, NULL, stress_producer, NULL);
    pthread_create(&cons, NULL, stress_consumer, &errors);
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    CHECK(errors == 0);
    CHECK(pcm_ring_used(&s_stress) == 0);
}

int main(void)
{
    test_init_rejects_bad_capacity();
    test_fill_and_drain();
    test_wraparound();
    test_threaded_stress();

    if (s_failures) {
        fprintf(stderr, "%d check(s) failed\n", s_failures);
        return 1;
    }
    printf("pcm_ring: all tests passed\n");
    return 0;
}
