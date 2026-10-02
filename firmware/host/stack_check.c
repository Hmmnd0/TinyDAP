/*
 * Measures the decoder's peak stack use on the host, the way FreeRTOS's
 * high-water mark does: run it on a thread whose stack is pre-filled with a
 * pattern, then count how much of the pattern was overwritten. Fails if peak
 * use exceeds a budget, so large stack buffers are caught here instead of
 * first appearing as a stack-overflow panic on the ESP32.
 *
 * Host frames differ from Xtensa, so treat the number as an estimate; the
 * default budget (20 KB) leaves margin inside the 24 KB decoder task
 * (minimp3 alone keeps a 16 KB scratch struct on the stack).
 *
 * Usage: stack_check <file> [budget-bytes]
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinydap/decoder.h"

#define STACK_BYTES (1024 * 1024)
#define FILL        0xA5

static const char *s_path;
static int s_result = 1;

static void *run(void *arg)
{
    (void)arg;
    decoder_info_t info;
    const char *err;
    decoder_t *d = decoder_open(s_path, &info, &err);
    if (!d) {
        fprintf(stderr, "%s: %s\n", s_path, err);
        return NULL;
    }
    static int16_t buf[DECODER_MAX_FRAMES * 2];
    size_t n, total = 0;
    while ((n = decoder_read(d, buf, DECODER_MAX_FRAMES)) > 0) {
        total += n;
    }
    decoder_close(d);
    /* MP3 lengths may be off by one frame (see decode_to_wav). */
    long diff = (long)total - (long)info.total_frames;
    long slack = strcmp(info.codec, "MP3") == 0 ? 1152 : 0;
    s_result = (diff >= -slack && diff <= slack) ? 0 : 2;
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file> [budget-bytes]\n", argv[0]);
        return 1;
    }
    s_path = argv[1];
    size_t budget = argc > 2 ? strtoul(argv[2], NULL, 0) : 20 * 1024;

    uint8_t *stack = aligned_alloc(16384, STACK_BYTES);
    if (!stack) {
        return 1;
    }
    memset(stack, FILL, STACK_BYTES);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstack(&attr, stack, STACK_BYTES);
    pthread_t t;
    pthread_create(&t, &attr, run, NULL);
    pthread_join(t, NULL);

    /* The stack grows down: untouched pattern remains at the low end. */
    size_t untouched = 0;
    while (untouched < STACK_BYTES && stack[untouched] == FILL) {
        untouched++;
    }
    size_t used = STACK_BYTES - untouched;
    free(stack);

    bool ok = s_result == 0 && used <= budget;
    printf("%s: peak stack %zu bytes (budget %zu)%s\n", ok ? "ok" : "FAILED", used, budget,
           s_result ? ", decode failed" : "");
    return ok ? 0 : 1;
}
