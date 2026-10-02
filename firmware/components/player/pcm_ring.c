#include "tinydap/pcm_ring.h"

#include <string.h>

bool pcm_ring_init(pcm_ring_t *r, void *storage, size_t capacity)
{
    if (!r || !storage || capacity == 0 || (capacity & (capacity - 1)) != 0) {
        return false;
    }
    r->buf = storage;
    r->mask = capacity - 1;
    atomic_init(&r->head, 0);
    atomic_init(&r->tail, 0);
    return true;
}

size_t pcm_ring_write(pcm_ring_t *r, const void *src, size_t len)
{
    size_t head = atomic_load_explicit(&r->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    size_t cap = r->mask + 1;
    size_t space = cap - (head - tail);
    if (len > space) {
        len = space;
    }

    size_t off = head & r->mask;
    size_t first = len < cap - off ? len : cap - off;
    memcpy(r->buf + off, src, first);
    memcpy(r->buf, (const uint8_t *)src + first, len - first);

    atomic_store_explicit(&r->head, head + len, memory_order_release);
    return len;
}

size_t pcm_ring_read(pcm_ring_t *r, void *dst, size_t len)
{
    size_t tail = atomic_load_explicit(&r->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    size_t avail = head - tail;
    if (len > avail) {
        len = avail;
    }

    size_t cap = r->mask + 1;
    size_t off = tail & r->mask;
    size_t first = len < cap - off ? len : cap - off;
    memcpy(dst, r->buf + off, first);
    memcpy((uint8_t *)dst + first, r->buf, len - first);

    atomic_store_explicit(&r->tail, tail + len, memory_order_release);
    return len;
}

void pcm_ring_reset(pcm_ring_t *r)
{
    atomic_store(&r->tail, atomic_load(&r->head));
}

size_t pcm_ring_used(pcm_ring_t *r)
{
    /* Load tail before head: head only grows and never falls behind tail,
     * so the difference can't go negative. It can briefly overshoot when
     * observed from a third task, hence the clamp. */
    size_t tail = atomic_load_explicit(&r->tail, memory_order_acquire);
    size_t head = atomic_load_explicit(&r->head, memory_order_acquire);
    size_t used = head - tail;
    size_t cap = r->mask + 1;
    return used > cap ? cap : used;
}

size_t pcm_ring_free(pcm_ring_t *r)
{
    return pcm_ring_capacity(r) - pcm_ring_used(r);
}
