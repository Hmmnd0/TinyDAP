#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Single-producer / single-consumer lock-free byte ring for PCM data.
 *
 * Exactly one task writes (the decoder) and exactly one task reads (audio
 * output). There are no locks, so the audio path can never block on a mutex
 * held by a lower-priority task (write-up §18: "Avoid holding locks in the
 * audio path").
 *
 * The caller supplies the storage so platform code decides where it lives
 * (internal SRAM on the ESP32-S3 for latency-critical buffers).
 */
typedef struct {
    uint8_t *buf;
    size_t mask;          /* capacity - 1; capacity is a power of two */
    atomic_size_t head;   /* total bytes ever written; stored by producer only */
    atomic_size_t tail;   /* total bytes ever read; stored by consumer only */
} pcm_ring_t;

/* Returns false if capacity is zero or not a power of two. */
bool pcm_ring_init(pcm_ring_t *r, void *storage, size_t capacity);

/* Producer side. Copies up to len bytes; returns the number copied. */
size_t pcm_ring_write(pcm_ring_t *r, const void *src, size_t len);

/* Consumer side. Copies up to len bytes; returns the number copied. */
size_t pcm_ring_read(pcm_ring_t *r, void *dst, size_t len);

/* Discards buffered data. Only safe while neither side is reading or writing. */
void pcm_ring_reset(pcm_ring_t *r);

/* Bytes currently buffered. Safe to call from any task (e.g. for stats). */
size_t pcm_ring_used(pcm_ring_t *r);

/* Bytes of free space. Safe to call from any task. */
size_t pcm_ring_free(pcm_ring_t *r);

static inline size_t pcm_ring_capacity(const pcm_ring_t *r) { return r->mask + 1; }
