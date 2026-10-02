/*
 * Drives the player UI on the host against a real folder, with a fake player,
 * and saves each screen as a BMP (4x scale) for checking layout.
 *
 * Usage: ui_demo <music-root> <out-dir> [keys]
 *   keys: u=up d=down s=select b=back p=play/pause n=next r=prev
 *         +=vol up -=vol down v=view toggle e=end current track
 *         t=repeat toggle
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tinydap/ui_app.h"
#include "tinydap/decoder.h"

#define SCALE 4

static player_status_t s_st;

static void fake_play(void *ctx, const char *path)
{
    (void)ctx;
    uint32_t id = s_st.track_id;
    memset(&s_st, 0, sizeof s_st);
    s_st.track_id = id + 1;
    snprintf(s_st.path, sizeof s_st.path, "%s", path);
    decoder_info_t info;
    const char *err = "can't open";
    decoder_t *d = decoder_open(path, &info, &err);
    if (d) {
        s_st.state = PLAYER_PLAYING;
        s_st.fmt = info.fmt;
        s_st.total_frames = info.total_frames;
        s_st.elapsed_frames = info.total_frames / 3;
        snprintf(s_st.codec, sizeof s_st.codec, "%s", info.codec);
        s_st.tags = info.tags;
        decoder_close(d);
    } else {
        s_st.state = PLAYER_ERROR;
        snprintf(s_st.error, sizeof s_st.error, "%s", err);
    }
}

static void fake_toggle(void *ctx)
{
    (void)ctx;
    s_st.state = s_st.state == PLAYER_PLAYING ? PLAYER_PAUSED : PLAYER_PLAYING;
}

static void fake_stop(void *ctx)
{
    (void)ctx;
    s_st.state = PLAYER_STOPPED;
}

static void fake_volume(void *ctx, int db)
{
    (void)ctx;
    (void)db;
}

static void fake_set_next(void *ctx, const char *path)
{
    (void)ctx;
    printf("next: %s\n", path ? path : "(none)");
}

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

/* 24-bit BMP, white-on-black like the OLED. */
static void save_bmp(const fb_t *fb, const char *path)
{
    int w = FB_W * SCALE, h = FB_H * SCALE;
    int row_bytes = (w * 3 + 3) & ~3;
    FILE *f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fwrite("BM", 1, 2, f);
    put32(f, 54 + (uint32_t)(row_bytes * h));
    put32(f, 0);
    put32(f, 54);
    put32(f, 40);
    put32(f, (uint32_t)w);
    put32(f, (uint32_t)h);
    put16(f, 1);
    put16(f, 24);
    put32(f, 0);
    put32(f, (uint32_t)(row_bytes * h));
    put32(f, 2835);
    put32(f, 2835);
    put32(f, 0);
    put32(f, 0);
    unsigned char *row = calloc(1, (size_t)row_bytes);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            unsigned char v = fb_get(fb, x / SCALE, y / SCALE) ? 0xFF : 0x10;
            row[x * 3] = row[x * 3 + 1] = row[x * 3 + 2] = v;
        }
        fwrite(row, 1, (size_t)row_bytes, f);
    }
    free(row);
    fclose(f);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <music-root> <out-dir> [keys]\n", argv[0]);
        return 1;
    }
    const char *keys = argc > 3 ? argv[3] : "";

    static ui_app_t app;
    player_ops_t ops = { fake_play, fake_toggle, fake_stop, fake_volume, fake_set_next, NULL };
    ui_app_init(&app, argv[1], true, &ops, -12);

    static fb_t fb;
    char path[512];
    uint32_t now = 0;
    for (int step = 0;; step++) {
        ui_app_tick(&app, &s_st, now);
        ui_app_render(&app, &fb, &s_st, now);
        snprintf(path, sizeof path, "%s/screen_%02d.bmp", argv[2], step);
        save_bmp(&fb, path);

        char k = keys[step];
        if (!k) {
            break;
        }
        ui_input_t in;
        switch (k) {
        case 'u': in = UI_UP; break;
        case 'd': in = UI_DOWN; break;
        case 's': in = UI_SELECT; break;
        case 'b': in = UI_BACK; break;
        case 'p': in = UI_PLAY_PAUSE; break;
        case 'n': in = UI_NEXT; break;
        case 'r': in = UI_PREV; break;
        case '+': in = UI_VOL_UP; break;
        case '-': in = UI_VOL_DOWN; break;
        case 'v': in = UI_VIEW_TOGGLE; break;
        case 't': in = UI_REPEAT; break;
        case 'e': s_st.state = PLAYER_ENDED; now += 5000; continue;
        default: continue;
        }
        ui_app_input(&app, in, &s_st, now);
        now += 5000;  /* let messages expire between steps */
    }
    printf("wrote screens to %s\n", argv[2]);
    return 0;
}
