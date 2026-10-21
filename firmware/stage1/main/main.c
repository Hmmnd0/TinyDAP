/*
 * TinyDAP Stage 1 bring-up on the ESP32-S3-DevKitC-1 breadboard.
 *
 * Checks each part of docs/Stage1_Wiring.md before the full player is
 * ported: the OLED shows a status screen, every button press is shown and
 * logged, and the PCM5102A plays a test tone.
 *
 * If flash_data/ held audio at build time, it is mounted read-only at
 * /flash and the first file plays on repeat through the real decoder.
 *
 *   Play  pause / resume
 *   Next  music -> tone: both -> tone: left only -> tone: right only
 *   Prev  restart the track (tone mode: change frequency)
 *   Vol+  / Vol-  level in 3 dB steps
 */

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "board_devkitc.h"
#include "buttons.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_sink.h"
#include "oled.h"
#include "tinydap/fb.h"
#include "tinydap/browser.h"
#include "tinydap/decoder.h"
#include "tinydap/tone.h"

static const char *TAG = "stage1";

#define SAMPLE_RATE 44100
#define CHUNK_FRAMES 256

typedef enum { MODE_MUSIC, OUT_BOTH, OUT_LEFT, OUT_RIGHT } out_mode_t;
static const char *OUT_NAME[] = { "Music", "Tone L+R", "Tone left", "Tone right" };
static const float FREQS[] = { 440.0f, 1000.0f, 220.0f };

static atomic_bool s_on = true;
static atomic_int s_out = OUT_BOTH;
static atomic_bool s_restart;
static atomic_uint s_played;       /* frames of the current track */
static char s_track[256];          /* empty: no music in flash */
static decoder_info_t s_info;
static atomic_int s_freq_i = 0;
static atomic_int s_level_db = -24;  /* gentle on headphones */

static void apply_gain(int16_t *buf, size_t samples, int db)
{
    float g = powf(10.0f, db / 20.0f);
    for (size_t i = 0; i < samples; i++) buf[i] = (int16_t)(buf[i] * g);
}

static void find_track(void)
{
    static browser_t b;  /* ~26 KB, keep it off the stack */
    if (browser_load(&b, "/flash") != 0) return;
    for (int i = 0; i < b.count; i++) {
        if (browser_kind(&b, i) != ENTRY_DIR && browser_entry_path(&b, i, s_track, sizeof s_track)) {
            return;
        }
    }
    s_track[0] = '\0';
}

static void audio_task(void *arg)
{
    audio_sink_t *sink = arg;
    audio_format_t fmt = { SAMPLE_RATE, 16, 2 };
    if (sink->open(sink, &fmt) != 0) {
        ESP_LOGE(TAG, "I2S open failed");
        vTaskDelete(NULL);
    }
    static int16_t buf[DECODER_MAX_FRAMES * 2];
    tone_t tone;
    int freq_i = -1;
    decoder_t *dec = NULL;
    for (;;) {
        bool on = atomic_load(&s_on);
        int out = atomic_load(&s_out);
        int level = atomic_load(&s_level_db);
        size_t frames = CHUNK_FRAMES;

        if (out == MODE_MUSIC && s_track[0]) {
            if (atomic_exchange(&s_restart, false) && dec) {
                decoder_close(dec);
                dec = NULL;
            }
            if (!dec) {
                const char *err = "";
                dec = decoder_open(s_track, &s_info, &err);
                if (!dec) {
                    ESP_LOGE(TAG, "open %s: %s", s_track, err);
                    atomic_store(&s_out, OUT_BOTH);
                    continue;
                }
                atomic_store(&s_played, 0);
                if (s_info.fmt.sample_rate != fmt.sample_rate) {
                    fmt.sample_rate = s_info.fmt.sample_rate;
                    sink->open(sink, &fmt);
                }
                ESP_LOGI(TAG, "playing %s (%s %u Hz)", s_track, s_info.codec, (unsigned)fmt.sample_rate);
            }
            if (!on) {
                memset(buf, 0, CHUNK_FRAMES * 4);
            } else {
                frames = decoder_read(dec, buf, DECODER_MAX_FRAMES);
                if (frames == 0) {  /* end of track: loop */
                    decoder_close(dec);
                    dec = NULL;
                    continue;
                }
                atomic_fetch_add(&s_played, frames);
                apply_gain(buf, frames * 2, level);
            }
        } else {
            if (dec) {
                decoder_close(dec);
                dec = NULL;
            }
            if (fmt.sample_rate != SAMPLE_RATE) {
                fmt.sample_rate = SAMPLE_RATE;
                sink->open(sink, &fmt);
            }
            if (freq_i != atomic_load(&s_freq_i)) {
                freq_i = atomic_load(&s_freq_i);
                tone_init(&tone, SAMPLE_RATE, FREQS[freq_i], 1.0f);
            }
            tone_fill_s16_stereo(&tone, buf, CHUNK_FRAMES);
            apply_gain(buf, CHUNK_FRAMES * 2, level);
            for (int i = 0; i < CHUNK_FRAMES; i++) {
                if (!on || out == OUT_RIGHT) buf[2 * i] = 0;
                if (!on || out == OUT_LEFT) buf[2 * i + 1] = 0;
            }
        }
        sink->write(sink, buf, frames * 4, 100);
    }
}

static void draw(fb_t *fb, button_t last, uint32_t presses)
{
    char line[40];
    fb_clear(fb);
    if (atomic_load(&s_out) == MODE_MUSIC && s_track[0]) {
        fb_fill_rect(fb, 0, 0, FB_W, 10, true);
        fb_text(fb, 2, 1, atomic_load(&s_on) ? "Now Playing" : "Paused", FB_W - 4, false);
        fb_text(fb, 2, 14, s_info.tags.title[0] ? s_info.tags.title : "Unknown", FB_W - 4, true);
        fb_text(fb, 2, 23, s_info.tags.artist, FB_W - 4, true);
        fb_text(fb, 2, 32, s_info.tags.album, FB_W - 4, true);
        uint32_t rate = s_info.fmt.sample_rate ? s_info.fmt.sample_rate : SAMPLE_RATE;
        uint32_t el = atomic_load(&s_played) / rate, tot = s_info.total_frames / rate;
        int w = tot ? (int)((FB_W - 4) * (uint64_t)el / tot) : 0;
        fb_rect(fb, 2, 43, FB_W - 4, 5, true);
        fb_fill_rect(fb, 2, 43, w, 5, true);
        snprintf(line, sizeof line, "%u:%02u/%u:%02u  %ddB", (unsigned)(el / 60), (unsigned)(el % 60),
                 (unsigned)(tot / 60), (unsigned)(tot % 60), atomic_load(&s_level_db));
        fb_text(fb, 2, 54, line, FB_W - 4, true);
        return;
    }
    fb_fill_rect(fb, 0, 0, FB_W, 10, true);
    fb_text(fb, 2, 1, "TinyDAP Stage 1", FB_W - 4, false);

    snprintf(line, sizeof line, "Tone %4d Hz  %s", (int)FREQS[atomic_load(&s_freq_i)],
             atomic_load(&s_on) ? "ON" : "off");
    fb_text(fb, 2, 13, line, FB_W - 4, true);
    snprintf(line, sizeof line, "Mode %s", OUT_NAME[atomic_load(&s_out)]);
    fb_text(fb, 2, 22, line, FB_W - 4, true);
    snprintf(line, sizeof line, "Level %d dB", atomic_load(&s_level_db));
    fb_text(fb, 2, 31, line, FB_W - 4, true);
    snprintf(line, sizeof line, "Last %s (GPIO %d) x%u", button_name(last), button_gpio(last),
             (unsigned)presses);
    fb_text(fb, 2, 40, line, FB_W - 4, true);

    /* Live button state: filled box = held down */
    static const char *lbl[BTN_COUNT] = { "P", "<", ">", "+", "-" };
    for (int i = 0; i < BTN_COUNT; i++) {
        int x = 4 + i * 25;
        if (button_is_down((button_t)i)) {
            fb_fill_rect(fb, x, 52, 20, 11, true);
            fb_text(fb, x + 7, 54, lbl[i], 10, false);
        } else {
            fb_rect(fb, x, 52, 20, 11, true);
            fb_text(fb, x + 7, 54, lbl[i], 10, true);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "TinyDAP Stage 1 bring-up on ESP32-S3-DevKitC-1");

    buttons_init();
    if (oled_init() != ESP_OK) {
        ESP_LOGE(TAG, "OLED init failed");
    }

    const esp_vfs_fat_mount_config_t mcfg = { .max_files = 2 };
    if (esp_vfs_fat_spiflash_mount_ro("/flash", "music", &mcfg) == ESP_OK) {
        find_track();
    }
    if (s_track[0]) {
        ESP_LOGI(TAG, "test track in flash: %s", s_track);
        atomic_store(&s_out, MODE_MUSIC);
        atomic_store(&s_level_db, -12);
    } else {
        ESP_LOGI(TAG, "no audio in flash; test tone only");
    }

    audio_sink_t *sink = i2s_sink_create(BOARD_I2S_PORT, BOARD_I2S_BCLK, BOARD_I2S_WS, BOARD_I2S_DOUT, -1);
    xTaskCreatePinnedToCore(audio_task, "audio", 16384, sink, 20, NULL, 1);

    /* A button that reads as held at boot is miswired (both legs in one row). */
    vTaskDelay(pdMS_TO_TICKS(50));
    for (int i = 0; i < 5; i++) buttons_poll();
    for (int i = 0; i < BTN_COUNT; i++) {
        if (button_is_down((button_t)i)) {
            ESP_LOGW(TAG, "%s (GPIO %d) reads as pressed at boot: check its legs are in two different rows",
                     button_name((button_t)i), button_gpio((button_t)i));
        }
    }

    static fb_t fb;
    button_t last = BTN_COUNT;
    uint32_t presses = 0;
    int64_t next_frame = 0;
    for (;;) {
        button_t b = buttons_poll();
        if (b != BTN_COUNT) {
            last = b;
            presses++;
            switch (b) {
            case BTN_PLAY: atomic_store(&s_on, !atomic_load(&s_on)); break;
            case BTN_NEXT: {
                int first = s_track[0] ? MODE_MUSIC : OUT_BOTH;
                int next = atomic_load(&s_out) + 1;
                atomic_store(&s_out, next > OUT_RIGHT ? first : next);
                break;
            }
            case BTN_PREV:
                if (atomic_load(&s_out) == MODE_MUSIC) atomic_store(&s_restart, true);
                else atomic_store(&s_freq_i, (atomic_load(&s_freq_i) + 1) % 3);
                break;
            case BTN_VOLUP: if (atomic_load(&s_level_db) < -6) atomic_fetch_add(&s_level_db, 3); break;
            case BTN_VOLDN: if (atomic_load(&s_level_db) > -45) atomic_fetch_sub(&s_level_db, 3); break;
            default: break;
            }
            ESP_LOGI(TAG, "button %s (GPIO %d): tone %s, %d Hz, %s, %d dB", button_name(b), button_gpio(b),
                     atomic_load(&s_on) ? "on" : "off", (int)FREQS[atomic_load(&s_freq_i)],
                     OUT_NAME[atomic_load(&s_out)], atomic_load(&s_level_db));
        }
        int64_t now = esp_timer_get_time();
        if (now >= next_frame) {
            draw(&fb, last, presses);
            oled_show(&fb);
            next_frame = now + 50000;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
