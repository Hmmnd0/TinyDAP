/*
 * TinyDAP firmware entry point — Stage 0 on the Cardputer-Adv.
 *
 * Browse the microSD card and play WAV files: keyboard -> input task -> UI
 * task -> player (decoder + audio tasks, §18) -> I2S -> ES8311 -> 3.5 mm jack.
 * The UI renders the final 128x64 OLED layout, scaled onto the ST7789.
 */

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "board_cardputer_adv.h"
#include "display.h"
#include "es8311.h"
#include "i2s_sink.h"
#include "keyboard.h"
#include "player.h"
#include "sdcard.h"
#include "tinydap/ui_app.h"

static const char *TAG = "tinydap";

#define DEFAULT_VOLUME_DB (-12)
#define UI_FRAME_MS       33
#define KEY_POLL_MS       10
#define REPEAT_DELAY_MS   400
#define REPEAT_RATE_MS    80

#define PRIO_INPUT  8
#define PRIO_UI     5
#define PRIO_STATS  2
#define CORE_SYSTEM 0

static QueueHandle_t s_inputs;   /* ui_input_t */
static ui_app_t s_app;
static fb_t s_fb;

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ---------- player ops for the UI ---------- */

static void ops_play(void *ctx, const char *path) { player_play(path); }
static void ops_toggle_pause(void *ctx) { player_toggle_pause(); }
static void ops_stop(void *ctx) { player_stop(); }
static void ops_volume(void *ctx, int db) { es8311_set_volume_db(db); }
static void ops_set_next(void *ctx, const char *path) { player_set_next(path); }

/* ---------- input: Cardputer keys -> UI events ---------- */

/* Same keys as EMBER where they overlap. */
static bool map_key(char key, ui_input_t *out)
{
    switch (key) {
    case ';': *out = UI_UP; return true;
    case '.': *out = UI_DOWN; return true;
    case '/':
    case KEY_ENTER: *out = UI_SELECT; return true;
    case ',':
    case '`':
    case KEY_BACKSPACE: *out = UI_BACK; return true;
    case ' ': *out = UI_PLAY_PAUSE; return true;
    case 'n': *out = UI_NEXT; return true;
    case 'b': *out = UI_PREV; return true;
    case '=': *out = UI_VOL_UP; return true;
    case '-': *out = UI_VOL_DOWN; return true;
    case 'm': *out = UI_VIEW_TOGGLE; return true;
    case 'r': *out = UI_REPEAT; return true;
    default: return false;
    }
}

static bool repeats(ui_input_t in)
{
    return in == UI_UP || in == UI_DOWN || in == UI_VOL_UP || in == UI_VOL_DOWN;
}

static void input_task(void *arg)
{
    key_event_t ev[8];
    char held = 0;
    uint32_t next_repeat = 0;

    for (;;) {
        int n = keyboard_read(ev, 8);
        for (int i = 0; i < n; i++) {
            ui_input_t in;
            if (!map_key(ev[i].key, &in)) {
                continue;
            }
            if (ev[i].pressed) {
                xQueueSend(s_inputs, &in, 0);
                if (repeats(in)) {
                    held = ev[i].key;
                    next_repeat = now_ms() + REPEAT_DELAY_MS;
                }
            } else if (ev[i].key == held) {
                held = 0;
            }
        }
        ui_input_t in;
        if (held && (int32_t)(now_ms() - next_repeat) >= 0 && map_key(held, &in)) {
            xQueueSend(s_inputs, &in, 0);
            next_repeat = now_ms() + REPEAT_RATE_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(KEY_POLL_MS));
    }
}

/* ---------- UI ---------- */

static void ui_task(void *arg)
{
    player_status_t st;
    for (;;) {
        ui_input_t in;
        bool got = xQueueReceive(s_inputs, &in, pdMS_TO_TICKS(UI_FRAME_MS)) == pdTRUE;
        player_get_status(&st);
        while (got) {
            ui_app_input(&s_app, in, &st, now_ms());
            player_get_status(&st);
            got = xQueueReceive(s_inputs, &in, 0) == pdTRUE;
        }
        ui_app_tick(&s_app, &st, now_ms());
        ui_app_render(&s_app, &s_fb, &st, now_ms());
        display_show(&s_fb);
    }
}

/*
 * Logs the §18 "What to measure" numbers. Decoder load is the share of wall
 * time spent decoding (and, within that, reading storage); "x realtime" is
 * how many times faster than playback the decoder runs while busy.
 */
static void stats_task(void *arg)
{
    static char tasks[1024];
    player_perf_t prev;
    player_get_perf(&prev);
    int64_t prev_us = esp_timer_get_time();

    for (int n = 0;; n++) {
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
        prev = perf;
        prev_us = now;

        unsigned load = wall ? (unsigned)((uint64_t)busy * 100 / wall) : 0;
        unsigned sd = wall ? (unsigned)((uint64_t)read * 100 / wall) : 0;
        unsigned rt10 = (busy && st.fmt.sample_rate)
            ? (unsigned)((uint64_t)frames * 10000000ull / st.fmt.sample_rate / busy) : 0;

        ESP_LOGI(TAG, "state %d  pcm fill %u%%  underruns %u  decode %u%% (sd %u%%) x%u.%u realtime  heap %u (min %u)",
                 st.state, st.buffer_pct, (unsigned)st.underruns, load, sd, rt10 / 10, rt10 % 10,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
        if (n % 6 == 0) {
            vTaskList(tasks);
            ESP_LOGI(TAG, "task  state prio stack-free num core\n%s", tasks);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "TinyDAP Stage 0 on %s", BOARD_NAME);

    /* Start I2S first: the ES8311 derives its clock from BCLK. */
    audio_sink_t *sink = i2s_sink_create(BOARD_CODEC_I2S_PORT, BOARD_CODEC_I2S_BCLK, BOARD_CODEC_I2S_WS,
                                         BOARD_CODEC_I2S_DOUT, BOARD_CODEC_I2S_MCLK);
    audio_format_t fmt = { .sample_rate = 44100, .bits_per_sample = 16, .channels = 2 };
    configASSERT(sink && sink->open(sink, &fmt) == 0);

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
        es8311_set_volume_db(DEFAULT_VOLUME_DB);
    }
    if (keyboard_init(i2c_bus, BOARD_KB_I2C_ADDR) != ESP_OK) {
        ESP_LOGE(TAG, "keyboard not found");
    }
    if (display_init() != ESP_OK) {
        ESP_LOGE(TAG, "display init failed");
    }
    bool sd_ok = sdcard_mount();

    /* The ES8311 is mono: fold stereo into it rather than dropping a channel. */
    player_start(sink, true);

    s_inputs = xQueueCreate(16, sizeof(ui_input_t));
    player_ops_t ops = { ops_play, ops_toggle_pause, ops_stop, ops_volume, ops_set_next, NULL };
    ui_app_init(&s_app, SDCARD_MOUNT, sd_ok, &ops, DEFAULT_VOLUME_DB);

    xTaskCreatePinnedToCore(input_task, "input", 3072, NULL, PRIO_INPUT, NULL, CORE_SYSTEM);
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, PRIO_UI, NULL, CORE_SYSTEM);
    xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, PRIO_STATS, NULL, CORE_SYSTEM);
}
