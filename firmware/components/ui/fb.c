#include "tinydap/fb.h"

#include <string.h>

#include "font5x7.h"

void fb_clear(fb_t *fb)
{
    memset(fb->buf, 0, sizeof fb->buf);
}

void fb_pixel(fb_t *fb, int x, int y, bool on)
{
    if (x < 0 || y < 0 || x >= FB_W || y >= FB_H) {
        return;
    }
    uint8_t *p = &fb->buf[(y / 8) * FB_W + x];
    uint8_t bit = (uint8_t)(1u << (y % 8));
    *p = on ? (*p | bit) : (*p & ~bit);
}

bool fb_get(const fb_t *fb, int x, int y)
{
    if (x < 0 || y < 0 || x >= FB_W || y >= FB_H) {
        return false;
    }
    return fb->buf[(y / 8) * FB_W + x] & (1u << (y % 8));
}

void fb_fill_rect(fb_t *fb, int x, int y, int w, int h, bool on)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            fb_pixel(fb, i, j, on);
        }
    }
}

void fb_invert_rect(fb_t *fb, int x, int y, int w, int h)
{
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            fb_pixel(fb, i, j, !fb_get(fb, i, j));
        }
    }
}

void fb_rect(fb_t *fb, int x, int y, int w, int h, bool on)
{
    fb_fill_rect(fb, x, y, w, 1, on);
    fb_fill_rect(fb, x, y + h - 1, w, 1, on);
    fb_fill_rect(fb, x, y, 1, h, on);
    fb_fill_rect(fb, x + w - 1, y, 1, h, on);
}

static void glyph(fb_t *fb, int x, int y, char c, bool on)
{
    unsigned char uc = (unsigned char)c;
    if (uc < 0x20 || uc > 0x7E) {
        uc = '?';
    }
    const uint8_t *g = font5x7[uc - 0x20];
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 7; row++) {
            if (g[col] & (1u << row)) {
                fb_pixel(fb, x + col, y + row, on);
            }
        }
    }
}

int fb_text_width(const char *s)
{
    return (int)strlen(s) * FONT_W;
}

int fb_text(fb_t *fb, int x, int y, const char *s, int max_w, bool on)
{
    int max_chars = max_w / FONT_W;
    int len = (int)strlen(s);
    for (int i = 0; i < len && i < max_chars; i++) {
        bool last_visible = (i == max_chars - 1) && (len > max_chars);
        glyph(fb, x, y, last_visible ? '~' : s[i], on);
        x += FONT_W;
    }
    return x;
}
