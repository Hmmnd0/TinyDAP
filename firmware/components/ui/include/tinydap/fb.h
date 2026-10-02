#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * 128 x 64 monochrome framebuffer in SSD1306 page layout: byte
 * (y / 8) * 128 + x holds column x of rows 8*(y/8) .. +7, LSB on top.
 * This is the final TinyDAP OLED's native format, so it can be sent to the
 * SSD1306 as-is; other displays (the Cardputer's ST7789) scale it.
 */
#define FB_W 128
#define FB_H 64

#define FONT_W 6   /* 5 px glyph + 1 px spacing */
#define FONT_H 8   /* 7 px glyph + 1 px spacing */

typedef struct {
    uint8_t buf[FB_W * FB_H / 8];
} fb_t;

void fb_clear(fb_t *fb);
void fb_pixel(fb_t *fb, int x, int y, bool on);
bool fb_get(const fb_t *fb, int x, int y);
void fb_fill_rect(fb_t *fb, int x, int y, int w, int h, bool on);
void fb_invert_rect(fb_t *fb, int x, int y, int w, int h);
void fb_rect(fb_t *fb, int x, int y, int w, int h, bool on);

/* Draws ASCII text with the 5x7 font, clipped to max_w pixels. If the text
 * doesn't fit, the last visible glyph becomes '~'. Returns the end x. */
int fb_text(fb_t *fb, int x, int y, const char *s, int max_w, bool on);

/* Pixel width of s in the 5x7 font. */
int fb_text_width(const char *s);
