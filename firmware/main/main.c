/*
 * TinyDAP firmware entry point — Stage 0 task layout (write-up §18).
 *
 * Current state: a test tone flows decoder -> PCM ring -> audio output ->
 * I2S -> ES8311 -> 3.5 mm jack (mono). Next: the storage task reads WAV from
 * microSD in place of the tone.
 */

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "board_cardputer_adv.h"
#include "es8311.h"
#include "i2s_sink.h"
#include "tinydap/audio_sink.h"
#include "tinydap/pcm_ring.h"
#include "tinydap/tone.h"

static const char *TAG = "tinydap";

#define SAMPLE_RATE      44100
#define FRAME_BYTES      4                      /* 16-bit stereo */
#define PCM_RING_BYTES   (32 * 1024)            /* ~186 ms at 44.1 kHz/16/2 */
#define AUDIO_TICK_MS    10
#define AUDIO_TICK_BYTES (SAMPLE_RATE / (1000 / AUDIO_TICK_MS) * FRAME_BYTES)

/* Tone at -20 dBFS, codec at 0 dB. The ES8311 output is quiet: M5Unified
 * applies 16x software gain on this board. */
#define TONE_AMPLITUDE   0.1f
#define CODEC_VOLUME_DB  0

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
static audio_sink_t *s_sink;

/* Highest priority: an underrun here is immediately audible. Pacing comes
 * from the sink's blocking write into the I2S DMA queue. */
static void audio_out_task(void *arg)
{
    static uint8_t buf[AUDIO_TICK_BYTES];

    /* Prebuffer so startup isn't counted as an underrun. */
    while (pcm_ring_used(&s_pcm) < pcm_ring_capacity(&s_pcm) / 2) {
        vTaskDelay(1);
    }

    for (;;) {
        size_t got = pcm_ring_read(&s_pcm, buf, sizeof buf);
        if (got < sizeof buf) {
            memset(buf + got, 0, sizeof buf - got);  /* pad with silence */
            atomic_fetch_add(&s_underruns, 1);
        }
        for (size_t off = 0; off < sizeof buf;) {
            int n = s_sink->write(s_sink, buf + off, sizeof buf - off, 100);
            if (n < 0) {
                ESP_LOGE(TAG, "audio sink write failed");
                break;
            }
            off += (size_t)n;
        }
    }
}

/* Keeps the PCM ring topped up ahead of the audio output. */
static void decoder_task(void *arg)
{
    int16_t chunk[256 * 2];
    tone_t tone;
    tone_init(&tone, SAMPLE_RATE, 440.0f, TONE_AMPLITUDE);

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

    /* Start I2S first: the ES8311 derives its clock from BCLK. */
    s_sink = i2s_sink_create(BOARD_CODEC_I2S_PORT, BOARD_CODEC_I2S_BCLK, BOARD_CODEC_I2S_WS,
                             BOARD_CODEC_I2S_DOUT, BOARD_CODEC_I2S_MCLK);
    audio_format_t fmt = { .sample_rate = SAMPLE_RATE, .bits_per_sample = 16, .channels = 2 };
    configASSERT(s_sink && s_sink->open(s_sink, &fmt) == 0);

    i2c_master_bus_handle_t i2c_bus;
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = BOARD_CODEC_I2C_SDA,
        .scl_io_num = BOARD_CODEC_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
    if (es8311_init(i2c_bus, BOARD_CODEC_I2C_ADDR) == ESP_OK) {
        es8311_set_volume_db(CODEC_VOLUME_DB);
    }

    spawn(decoder_task,   "decoder",   8192, PRIO_DECODER,   CORE_AUDIO,  T_DECODER);
    spawn(audio_out_task, "audio_out", 4096, PRIO_AUDIO_OUT, CORE_AUDIO,  T_AUDIO_OUT);
    spawn(storage_task,   "storage",   4096, PRIO_STORAGE,   CORE_SYSTEM, T_STORAGE);
    spawn(input_task,     "input",     3072, PRIO_INPUT,     CORE_SYSTEM, T_INPUT);
    spawn(ui_task,        "ui",        4096, PRIO_UI,        CORE_SYSTEM, T_UI);
    spawn(stats_task,     "stats",     4096, PRIO_STATS,     CORE_SYSTEM, T_STATS);
}
