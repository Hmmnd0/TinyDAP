/*
 * TinyDAP Stage 1 on the ESP32-S3-DevKitC-1 breadboard (docs/Stage1_Wiring.md).
 *
 * The same player and UI as Stage 0 (components/ui, ../main/player.c), on
 * the final kind of hardware: SSD1306 OLED, five buttons, PCM5102A stereo,
 * microSD over 4-bit SDMMC. If the card doesn't mount, the browser opens
 * the test audio built into flash (flash_data/) instead.
 *
 * Buttons, browser:      Vol+/Vol- up/down (hold to scroll), Next open/play,
 *                        Prev back, Play play/pause, hold Play: Now Playing
 * Buttons, Now Playing:  Vol+/Vol- volume, Next/Prev track, Play play/pause,
 *                        hold Play: browser, hold Next: repeat on/off
 */

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "board_devkitc.h"
#include "buttons.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "i2s_sink.h"
#include "oled.h"
#include "player.h"
#include "sdcard.h"
#include "tinydap/ui_app.h"

static const char *TAG = "tinydap";

#define DEFAULT_VOLUME_DB   (-20)
#define UI_FRAME_MS         33
#define FLASH_MOUNT         "/flash"

/* Burn-in protection (write-up §8): dim, and off after inactivity. The
 * timeout is off for the breadboard: with five buttons there's no spare one
 * to wake the screen without also acting. 0 disables it. */
#define OLED_CONTRAST       0x7F
#define SCREEN_TIMEOUT_MS   0

#define PRIO_INPUT  8
#define PRIO_UI     5
#define PRIO_STATS  2
#define CORE_SYSTEM 0

static ui_app_t s_app;
static fb_t s_fb;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ---------- digital volume ---------- */

/* The PCM5102A has no volume control, so gain is applied in software on
 * the way to I2S. Rev A's CS43131 will do this in hardware. */
typedef struct {
    audio_sink_t base;
    audio_sink_t *out;
} gain_sink_t;

static atomic_int s_gain_q15 = 32767;  /* gain as Q15 */

static int gain_open(audio_sink_t *self, const audio_format_t *fmt)
{
    gain_sink_t *g = self->ctx;
    return g->out->open(g->out, fmt);
}

static int gain_write(audio_sink_t *self, const void *data, size_t len, uint32_t timeout_ms)
{
    gain_sink_t *g = self->ctx;
    int32_t q = atomic_load(&s_gain_q15);
    if (q >= 32767) {
        return g->out->write(g->out, data, len, timeout_ms);
    }
    /* Scale a copy: the caller may retry the unwritten tail of its buffer. */
    static int16_t tmp[1024];
    size_t n = len < sizeof tmp ? len : sizeof tmp;
    const int16_t *src = data;
    for (size_t i = 0; i < n / 2; i++) {
        tmp[i] = (int16_t)((src[i] * q) >> 15);
    }
    return g->out->write(g->out, tmp, n, timeout_ms);
}

static void gain_close(audio_sink_t *self)
{
    gain_sink_t *g = self->ctx;
    g->out->close(g->out);
}

static audio_sink_t *gain_sink_create(audio_sink_t *out)
{
    gain_sink_t *g = calloc(1, sizeof *g);
    g->out = out;
    g->base = (audio_sink_t){ gain_open, gain_write, gain_close, g };
    return &g->base;
}

static void set_volume_db(int db)
{
    atomic_store(&s_gain_q15, db >= 0 ? 32767 : (int)(32767.0f * powf(10.0f, db / 20.0f)));
}

/* ---------- player ops for the UI ---------- */

static void ops_play(void *ctx, const char *path) { player_play(path); }
static void ops_toggle_pause(void *ctx) { player_toggle_pause(); }
static void ops_stop(void *ctx) { player_stop(); }
static void ops_volume(void *ctx, int db) { set_volume_db(db); }
static void ops_set_next(void *ctx, const char *path) { player_set_next(path); }

/* ---------- input: five buttons -> UI events ---------- */

static QueueHandle_t s_raw;  /* button_event_t, mapped in the UI task (it knows the view) */

static void input_task(void *arg)
{
    for (;;) {
        button_event_t e = buttons_poll(now_ms());
        if (e.ev != BTN_EV_NONE) {
            ESP_LOGI(TAG, "button %s %s", button_name(e.button),
                     e.ev == BTN_EV_LONG ? "long" : e.ev == BTN_EV_REPEAT ? "repeat" : "short");
            xQueueSend(s_raw, &e, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(BUTTONS_POLL_MS));
    }
}

/* Returns false if the event maps to nothing. */
static bool map_button(button_event_t e, ui_view_t view, ui_input_t *out)
{
    bool browser = view == VIEW_BROWSER;
    switch (e.button) {
    case BTN_VOLUP: *out = browser ? UI_UP : UI_VOL_UP; return true;
    case BTN_VOLDN: *out = browser ? UI_DOWN : UI_VOL_DOWN; return true;
    case BTN_PLAY: *out = e.ev == BTN_EV_LONG ? UI_VIEW_TOGGLE : UI_PLAY_PAUSE; return true;
    case BTN_NEXT:
        if (e.ev == BTN_EV_LONG) {
            *out = UI_REPEAT;
        } else {
            *out = browser ? UI_SELECT : UI_NEXT;
        }
        return true;
    case BTN_PREV:
        if (e.ev == BTN_EV_LONG) {
            return false;
        }
        *out = browser ? UI_BACK : UI_PREV;
        return true;
    default:
        return false;
    }
}

/* ---------- UI ---------- */

static void ui_task(void *arg)
{
    player_status_t st;
    bool screen_on = true;
    uint32_t last_input = now_ms();
    for (;;) {
        button_event_t e;
        bool got = xQueueReceive(s_raw, &e, pdMS_TO_TICKS(UI_FRAME_MS)) == pdTRUE;
        player_get_status(&st);
        while (got) {
            /* With the screen off, Now Playing's buttons still control
             * playback (a DAP in a pocket); in the browser a press only
             * wakes the screen. */
            bool act = screen_on || s_app.view == VIEW_NOW_PLAYING;
            ui_input_t in;
            if (act && map_button(e, s_app.view, &in)) {
                ui_app_input(&s_app, in, &st, now_ms());
                player_get_status(&st);
            }
            if (screen_on || !act) {
                if (!screen_on) {
                    oled_set_on(true);
                    screen_on = true;
                }
                last_input = now_ms();
            }
            got = xQueueReceive(s_raw, &e, 0) == pdTRUE;
        }
#if SCREEN_TIMEOUT_MS > 0
        if (screen_on && now_ms() - last_input >= SCREEN_TIMEOUT_MS) {
            oled_set_on(false);
            screen_on = false;
        }
#endif
        ui_app_tick(&s_app, &st, now_ms());
        if (screen_on) {
            ui_app_render(&s_app, &s_fb, &st, now_ms());
            oled_show(&s_fb);
        }
    }
}

/* Logs the §18 "What to measure" numbers every 5 s (as on Stage 0). */
static void stats_task(void *arg)
{
    player_perf_t prev;
    player_get_perf(&prev);
    int64_t prev_us = esp_timer_get_time();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        player_status_t st;
        player_perf_t perf;
        player_get_status(&st);
        player_get_perf(&perf);
        int64_t now = esp_timer_get_time();
        uint32_t wall = (uint32_t)(now - prev_us);
        uint32_t busy = perf.busy_us - prev.busy_us;
        uint32_t read = perf.read_us - prev.read_us;
        uint32_t frames = perf.frames - prev.frames;
        uint32_t rbytes = perf.read_bytes - prev.read_bytes;
        uint32_t rcalls = perf.read_calls - prev.read_calls;
        prev = perf;
        prev_us = now;
        if (st.state != PLAYER_PLAYING) {
            continue;
        }
        unsigned load = wall ? (unsigned)((uint64_t)busy * 100 / wall) : 0;
        unsigned sd = wall ? (unsigned)((uint64_t)read * 100 / wall) : 0;
        unsigned rt10 = (busy && st.fmt.sample_rate)
            ? (unsigned)((uint64_t)frames * 10000000ull / st.fmt.sample_rate / busy) : 0;
        unsigned sd_kbps = read ? (unsigned)((uint64_t)rbytes * 1000 / read) : 0;
        unsigned per_read = rcalls ? (unsigned)(rbytes / rcalls) : 0;
        ESP_LOGI(TAG, "pcm fill %u%%  underruns %u  decode %u%% (sd %u%%, %u KB/s, %u B/read) "
                      "x%u.%u realtime  heap %u (min %u)",
                 st.buffer_pct, (unsigned)st.underruns, load, sd, sd_kbps, per_read, rt10 / 10, rt10 % 10,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "TinyDAP Stage 1 on ESP32-S3-DevKitC-1");

    buttons_init();
    if (oled_init() != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed");
    }
    oled_set_contrast(OLED_CONTRAST);

    audio_sink_t *i2s = i2s_sink_create(BOARD_I2S_PORT, BOARD_I2S_BCLK, BOARD_I2S_WS, BOARD_I2S_DOUT, -1);
    audio_sink_t *sink = gain_sink_create(i2s);
    audio_format_t fmt = { .sample_rate = 44100, .bits_per_sample = 16, .channels = 2 };
    configASSERT(sink->open(sink, &fmt) == 0);
    set_volume_db(DEFAULT_VOLUME_DB);

    const char *root = SDCARD_MOUNT;
    bool storage_ok = sdcard_mount();
    if (!storage_ok) {
        const esp_vfs_fat_mount_config_t mcfg = { .max_files = 2 };
        if (esp_vfs_fat_spiflash_mount_ro(FLASH_MOUNT, "music", &mcfg) == ESP_OK) {
            ESP_LOGW(TAG, "no SD card: browsing the test audio in flash");
            root = FLASH_MOUNT;
            storage_ok = true;
        }
    }

    /* PCM5102A is stereo: no downmix (the Cardputer's ES8311 needed one). */
    player_start(sink, false);

    s_raw = xQueueCreate(16, sizeof(button_event_t));
    player_ops_t ops = { ops_play, ops_toggle_pause, ops_stop, ops_volume, ops_set_next, NULL };
    ui_app_init(&s_app, root, storage_ok, &ops, DEFAULT_VOLUME_DB);

    xTaskCreatePinnedToCore(input_task, "input", 3072, NULL, PRIO_INPUT, NULL, CORE_SYSTEM);
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, PRIO_UI, NULL, CORE_SYSTEM);
    xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, PRIO_STATS, NULL, CORE_SYSTEM);
}
