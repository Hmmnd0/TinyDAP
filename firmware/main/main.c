/*
 * TinyDAP firmware entry point — Stage 0 task layout (write-up §18).
 *
 * Current state: a test tone flows decoder -> PCM ring -> audio output, where
 * audio output is a "null sink" that drains the ring at the real 44.1 kHz rate
 * and counts underruns. Step 3 replaces the null sink with I2S -> ES8311, and
 * the storage task gets SD + WAV reading.
 */

#include <stdatomic.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "board_cardputer_adv.h"
#include "tinydap/pcm_ring.h"
#include "tinydap/tone.h"

static const char *TAG = "tinydap";

#define SAMPLE_RATE      44100
#define FRAME_BYTES      4                      /* 16-bit stereo */
#define PCM_RING_BYTES   (32 * 1024)            /* ~186 ms at 44.1 kHz/16/2 */
#define AUDIO_TICK_MS    10
#define AUDIO_TICK_BYTES (SAMPLE_RATE / (1000 / AUDIO_TICK_MS) * FRAME_BYTES)

/* Core 1 runs the audio pipeline; core 0 runs everything else (§18). */
#define CORE_AUDIO  1
#define CORE_SYSTEM 0

#define PRIO_AUDIO_OUT (configMAX_PRIORITIES - 2)
#define PRIO_DECODER   (configMAX_PRIORITIES - 3)
#define PRIO_STORAGE   (configMAX_PRIORITIES - 4)
#define PRIO_INPUT     8
#define PRIO_UI        5
#define PRIO_STATS     2

enum { T_AUDIO_OUT, T_DECODER, T_STORAGE, T_INPUT, T_UI, T_STATS, T_COUNT };
static TaskHandle_t s_tasks[T_COUNT];

static pcm_ring_t s_pcm;
static atomic_uint s_underruns;

/* Highest priority: an underrun here is immediately audible. */
static void audio_out_task(void *arg)
{
    static uint8_t buf[AUDIO_TICK_BYTES];
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        /* Null sink: consume one tick of audio at the real playback rate.
         * TODO(step 3): replace with i2s_channel_write() to the ES8311, whose
         * blocking DMA write then provides the pacing instead of the delay. */
        vTaskDelayUntil(&last, pdMS_TO_TICKS(AUDIO_TICK_MS));
        if (pcm_ring_read(&s_pcm, buf, sizeof buf) < sizeof buf) {
            atomic_fetch_add(&s_underruns, 1);
        }
    }
}

/* Keeps the PCM ring topped up ahead of the audio output. */
static void decoder_task(void *arg)
{
    int16_t chunk[256 * 2];
    tone_t tone;
    tone_init(&tone, SAMPLE_RATE, 440.0f, 0.1f);

    for (;;) {
        /* TODO(step 4): decode FLAC/MP3 from the compressed-data buffer. */
        if (pcm_ring_free(&s_pcm) >= sizeof chunk) {
            tone_fill_s16_stereo(&tone, chunk, 256);
            pcm_ring_write(&s_pcm, chunk, sizeof chunk);
        } else {
            vTaskDelay(1);
        }
    }
}

static void storage_task(void *arg)
{
    /* TODO(step 3): mount microSD, read-ahead file data into a compressed
     * buffer for the decoder. */
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void input_task(void *arg)
{
    /* TODO: keyboard -> player command queue (play/pause, prev/next, volume). */
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

static void ui_task(void *arg)
{
    /* TODO: on-screen diagnostics (§17): metadata, buffer fill, underruns. */
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

/* Logs the §18 "What to measure" numbers available so far. */
static void stats_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        unsigned fill_pct = (unsigned)(pcm_ring_used(&s_pcm) * 100 / pcm_ring_capacity(&s_pcm));
        ESP_LOGI(TAG, "pcm fill %u%%  underruns %u  internal heap free %u (min %u)",
                 fill_pct,
                 atomic_load(&s_underruns),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));

        for (int i = 0; i < T_COUNT; i++) {
            ESP_LOGI(TAG, "  %-10s stack free %u B", pcTaskGetName(s_tasks[i]),
                     (unsigned)uxTaskGetStackHighWaterMark(s_tasks[i]));
        }
    }
}

static void spawn(TaskFunction_t fn, const char *name, uint32_t stack_bytes,
                  UBaseType_t prio, BaseType_t core, int slot)
{
    BaseType_t ok = xTaskCreatePinnedToCore(fn, name, stack_bytes, NULL, prio,
                                            &s_tasks[slot], core);
    configASSERT(ok == pdPASS);
}

void app_main(void)
{
    ESP_LOGI(TAG, "TinyDAP Stage 0 on %s", BOARD_NAME);

    /* Latency-critical PCM stays in internal SRAM, never PSRAM (§2, §18). */
    void *ring_mem = heap_caps_malloc(PCM_RING_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    configASSERT(ring_mem && pcm_ring_init(&s_pcm, ring_mem, PCM_RING_BYTES));

    spawn(audio_out_task, "audio_out", 4096, PRIO_AUDIO_OUT, CORE_AUDIO,  T_AUDIO_OUT);
    spawn(decoder_task,   "decoder",   8192, PRIO_DECODER,   CORE_AUDIO,  T_DECODER);
    spawn(storage_task,   "storage",   4096, PRIO_STORAGE,   CORE_SYSTEM, T_STORAGE);
    spawn(input_task,     "input",     3072, PRIO_INPUT,     CORE_SYSTEM, T_INPUT);
    spawn(ui_task,        "ui",        4096, PRIO_UI,        CORE_SYSTEM, T_UI);
    spawn(stats_task,     "stats",     4096, PRIO_STATS,     CORE_SYSTEM, T_STATS);
}
