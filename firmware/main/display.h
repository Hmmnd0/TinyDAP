#pragma once

#include "esp_err.h"
#include "tinydap/fb.h"

/*
 * Cardputer-Adv ST7789 shown as a stand-in for the 128x64 OLED: the mono
 * framebuffer is scaled to 240x128 (1.875x by 2) and centered vertically.
 */
esp_err_t display_init(void);

/* Pushes the framebuffer if it changed since the last call. */
void display_show(const fb_t *fb);
