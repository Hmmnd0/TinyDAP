/*
 * TinyDAP Stage 1 bring-up on the ESP32-S3-DevKitC-1 breadboard.
 *
 * Checks each part of docs/Stage1_Wiring.md before the full player is
 * ported: the OLED shows a status screen, every button press is shown and
 * logged, and the PCM5102A plays a test tone.
 *
 *   Play  tone on/off
 *   Next  output: both -> left only -> right only
 *   Prev  frequency: 440 -> 1000 -> 220 Hz
 *   Vol+  / Vol-  level in 3 dB steps
 */

#include <math.h>
#include <stdatomic.h>
#include <stdio.h>

#include "board_devkitc.h"
#include "buttons.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2s_sink.h"
#include "oled.h"
#include "tinydap/fb.h"
#include "tinydap/tone.h"

static const char *TAG = "stage1";

#define SAMPLE_RATE 44100
#define CHUNK_FRAMES 256

typedef enum { OUT_BOTH, OUT_LEFT, OUT_RIGHT } out_mode_t;
static const char *OUT_NAME[] = { "Both", "Left only", "Right only" };
static const float FREQS[] = { 440.0f, 1000.0f, 220.0f };

static atomic_bool s_on = true;
static atomic_int s_out = OUT_BOTH;
static atomic_int s_freq_i = 0;
static atomic_int s_level_db = -24;  /* gentle on headphones */

static void audio_task(void *arg)
{
    audio_sink_t *sink = arg;
    audio_format_t fmt = { SAMPLE_RATE, 16, 2 };
    if (sink->open(sink, &fmt) != 0) {
        ESP_LOGE(TAG, "I2S open failed");
        vTaskDelete(NULL);
    }
    static int16_t buf[CHUNK_FRAMES * 2];
    tone_t tone;
    int freq_i = -1, level = 1;
    for (;;) {
        if (freq_i != atomic_load(&s_freq_i) || level != atomic_load(&s_level_db)) {
            freq_i = atomic_load(&s_freq_i);
            level = atomic_load(&s_level_db);
            tone_init(&tone, SAMPLE_RATE, FREQS[freq_i], powf(10.0f, level / 20.0f));
        }
        tone_fill_s16_stereo(&tone, buf, CHUNK_FRAMES);
        bool on = atomic_load(&s_on);
        int out = atomic_load(&s_out);
        for (int i = 0; i < CHUNK_FRAMES; i++) {
            if (!on || out == OUT_RIGHT) buf[2 * i] = 0;
            if (!on || out == OUT_LEFT) buf[2 * i + 1] = 0;
        }
        sink->write(sink, buf, sizeof buf, 100);
    }
}

static void draw(fb_t *fb, button_t last, uint32_t presses)
{
    char line[32];
    fb_clear(fb);
    fb_fill_rect(fb, 0, 0, FB_W, 10, true);
    fb_text(fb, 2, 1, "TinyDAP Stage 1", FB_W - 4, false);

    snprintf(line, sizeof line, "Tone %4d Hz  %s", (int)FREQS[atomic_load(&s_freq_i)],
             atomic_load(&s_on) ? "ON" : "off");
    fb_text(fb, 2, 13, line, FB_W - 4, true);
    snprintf(line, sizeof line, "Out  %s", OUT_NAME[atomic_load(&s_out)]);
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

    audio_sink_t *sink = i2s_sink_create(BOARD_I2S_PORT, BOARD_I2S_BCLK, BOARD_I2S_WS, BOARD_I2S_DOUT, -1);
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, sink, 20, NULL, 1);

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
            case BTN_NEXT: atomic_store(&s_out, (atomic_load(&s_out) + 1) % 3); break;
            case BTN_PREV: atomic_store(&s_freq_i, (atomic_load(&s_freq_i) + 1) % 3); break;
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
