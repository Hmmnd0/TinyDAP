#include "player.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tinydap/pcm_ring.h"
#include "tinydap/decoder.h"

static const char *TAG = "player";

#define PCM_RING_BYTES   (32 * 1024)     /* ~186 ms at 44.1 kHz/16/2 */
#define OUT_FRAME_BYTES  4               /* ring holds 16-bit stereo */
#define OUT_CHUNK_FRAMES 256
#define DECODE_FRAMES    DECODER_MAX_FRAMES

/* Core 1 runs the audio pipeline; core 0 runs everything else (§18). */
#define CORE_AUDIO     1
#define PRIO_AUDIO_OUT (configMAX_PRIORITIES - 2)
#define PRIO_DECODER   (configMAX_PRIORITIES - 3)

typedef enum { CMD_PLAY, CMD_TOGGLE_PAUSE, CMD_STOP } cmd_type_t;

typedef struct {
    cmd_type_t type;
    char path[256];
} player_cmd_t;

static audio_sink_t *s_sink;
static bool s_downmix;
static pcm_ring_t s_ring;
static QueueHandle_t s_cmds;

static SemaphoreHandle_t s_lock;     /* guards s_status */
static player_status_t s_status;

/*
 * Audio-output run/idle handshake. The decoder clears s_out_run and bumps
 * s_idle_gen; the audio task acknowledges via s_idle_ack once it has stopped
 * touching the ring and the sink, so the decoder can then reset the ring or
 * retune the I2S clock safely.
 */
static atomic_bool s_out_run;
static atomic_uint s_idle_gen;
static SemaphoreHandle_t s_idle_ack;

static atomic_bool s_eof;             /* current track fully decoded */
static atomic_uint s_underruns;
static atomic_uint s_frames_written;  /* frames into the ring this track */

/* Decoder-task state */
static decoder_t *s_dec;
static bool s_file_done;
static uint32_t s_sink_rate = 44100;
static int16_t s_out[DECODE_FRAMES * 2];

/* ---------- audio output task ---------- */

static void audio_out_task(void *arg)
{
    static uint8_t buf[OUT_CHUNK_FRAMES * OUT_FRAME_BYTES];
    bool running = false;
    unsigned acked = 0;

    for (;;) {
        unsigned gen = atomic_load(&s_idle_gen);
        if (!atomic_load(&s_out_run)) {
            running = false;
            if (gen != acked) {
                acked = gen;
                xSemaphoreGive(s_idle_ack);
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (!running) {
            /* Prebuffer so a (re)start isn't counted as an underrun. */
            if (pcm_ring_used(&s_ring) < pcm_ring_capacity(&s_ring) / 2 && !atomic_load(&s_eof)) {
                vTaskDelay(1);
                continue;
            }
            running = true;
        }

        size_t got = pcm_ring_read(&s_ring, buf, sizeof buf);
        if (got < sizeof buf) {
            memset(buf + got, 0, sizeof buf - got);  /* pad with silence */
            if (!atomic_load(&s_eof)) {
                atomic_fetch_add(&s_underruns, 1);
            }
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

static void out_stop(void)
{
    atomic_store(&s_out_run, false);
    atomic_fetch_add(&s_idle_gen, 1);
    xSemaphoreTake(s_idle_ack, portMAX_DELAY);
}

static void out_start(void)
{
    atomic_store(&s_out_run, true);
}

/* ---------- decoder task ---------- */

static void set_state(player_state_t state, const char *error)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.state = state;
    snprintf(s_status.error, sizeof s_status.error, "%s", error ? error : "");
    xSemaphoreGive(s_lock);
}

static void close_file(void)
{
    decoder_close(s_dec);
    s_dec = NULL;
}

static void stop_output_and_flush(void)
{
    out_stop();
    pcm_ring_reset(&s_ring);
    close_file();
    atomic_store(&s_frames_written, 0);
    atomic_store(&s_eof, false);
    s_file_done = false;
}

static void start_track(const char *path)
{
    stop_output_and_flush();

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.track_id++;
    snprintf(s_status.path, sizeof s_status.path, "%s", path);
    memset(&s_status.fmt, 0, sizeof s_status.fmt);
    memset(&s_status.tags, 0, sizeof s_status.tags);
    s_status.codec[0] = '\0';
    s_status.total_frames = 0;
    xSemaphoreGive(s_lock);

    const char *err = NULL;
    decoder_info_t info;
    decoder_t *dec = decoder_open(path, &info, &err);

    if (dec && info.fmt.sample_rate != s_sink_rate) {
        audio_format_t out = { .sample_rate = info.fmt.sample_rate, .bits_per_sample = 16, .channels = 2 };
        if (s_sink->open(s_sink, &out) == 0) {
            s_sink_rate = out.sample_rate;
        } else {
            err = "Rate change failed";
            decoder_close(dec);
            dec = NULL;
        }
    }
    if (!dec) {
        ESP_LOGW(TAG, "%s: %s", path, err);
        set_state(PLAYER_ERROR, err);
        return;
    }

    s_dec = dec;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_status.fmt = info.fmt;
    s_status.total_frames = info.total_frames;
    snprintf(s_status.codec, sizeof s_status.codec, "%s", info.codec);
    s_status.tags = info.tags;
    s_status.state = PLAYER_PLAYING;
    s_status.error[0] = '\0';
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "playing %s (%s %u Hz, %u-bit, %u ch)", path, info.codec,
             (unsigned)info.fmt.sample_rate, info.fmt.bits_per_sample, info.fmt.channels);
    out_start();
}

/* Decodes a block into the ring, folding to mono if needed. */
static void decode_chunk(void)
{
    size_t got = decoder_read(s_dec, s_out, DECODE_FRAMES);
    if (got == 0) {
        s_file_done = true;
        atomic_store(&s_eof, true);
        return;
    }
    if (s_downmix) {
        for (size_t i = 0; i < got; i++) {
            int16_t m = (int16_t)((s_out[2 * i] + s_out[2 * i + 1]) / 2);
            s_out[2 * i] = s_out[2 * i + 1] = m;
        }
    }
    pcm_ring_write(&s_ring, s_out, got * OUT_FRAME_BYTES);
    atomic_fetch_add(&s_frames_written, (unsigned)got);
}

static void handle_command(const player_cmd_t *cmd)
{
    switch (cmd->type) {
    case CMD_PLAY:
        start_track(cmd->path);
        break;
    case CMD_TOGGLE_PAUSE: {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        player_state_t st = s_status.state;
        xSemaphoreGive(s_lock);
        if (st == PLAYER_PLAYING) {
            out_stop();
            set_state(PLAYER_PAUSED, NULL);
        } else if (st == PLAYER_PAUSED) {
            set_state(PLAYER_PLAYING, NULL);
            out_start();
        }
        break;
    }
    case CMD_STOP:
        stop_output_and_flush();
        set_state(PLAYER_STOPPED, NULL);
        break;
    }
}

static void decoder_task(void *arg)
{
    for (;;) {
        bool can_decode = s_dec && !s_file_done && pcm_ring_free(&s_ring) >= sizeof s_out;
        player_cmd_t cmd;
        if (xQueueReceive(s_cmds, &cmd, can_decode ? 0 : pdMS_TO_TICKS(5)) == pdTRUE) {
            handle_command(&cmd);
            continue;
        }
        if (can_decode) {
            decode_chunk();
        } else if (s_dec && s_file_done && pcm_ring_used(&s_ring) == 0) {
            /* Everything decoded has been sent; let the DMA queue drain. */
            vTaskDelay(pdMS_TO_TICKS(50));
            out_stop();
            close_file();
            set_state(PLAYER_ENDED, NULL);
        }
    }
}

/* ---------- public API ---------- */

void player_start(audio_sink_t *sink, bool mono_downmix)
{
    s_sink = sink;
    s_downmix = mono_downmix;

    /* Latency-critical PCM stays in internal SRAM, never PSRAM (§2, §18). */
    void *mem = heap_caps_malloc(PCM_RING_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    configASSERT(mem && pcm_ring_init(&s_ring, mem, PCM_RING_BYTES));
    s_cmds = xQueueCreate(4, sizeof(player_cmd_t));
    s_lock = xSemaphoreCreateMutex();
    s_idle_ack = xSemaphoreCreateBinary();
    configASSERT(s_cmds && s_lock && s_idle_ack);

    BaseType_t ok = xTaskCreatePinnedToCore(audio_out_task, "audio_out", 4096, NULL,
                                            PRIO_AUDIO_OUT, NULL, CORE_AUDIO);
    /* dr_flac decodes on the stack: ~7.2 KB measured for 16/44.1 FLAC. */
    ok &= xTaskCreatePinnedToCore(decoder_task, "decoder", 16384, NULL, PRIO_DECODER, NULL, CORE_AUDIO);
    configASSERT(ok == pdPASS);
}

static void send(cmd_type_t type, const char *path)
{
    player_cmd_t cmd = { .type = type };
    if (path) {
        snprintf(cmd.path, sizeof cmd.path, "%s", path);
    }
    if (xQueueSend(s_cmds, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full");
    }
}

void player_play(const char *path) { send(CMD_PLAY, path); }
void player_toggle_pause(void) { send(CMD_TOGGLE_PAUSE, NULL); }
void player_stop(void) { send(CMD_STOP, NULL); }

void player_get_status(player_status_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_status;
    xSemaphoreGive(s_lock);

    size_t used = pcm_ring_used(&s_ring);
    uint32_t written = atomic_load(&s_frames_written);
    uint32_t buffered = (uint32_t)(used / OUT_FRAME_BYTES);
    out->elapsed_frames = written > buffered ? written - buffered : 0;
    out->underruns = atomic_load(&s_underruns);
    out->buffer_pct = (uint8_t)(used * 100 / pcm_ring_capacity(&s_ring));
}
