#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "tinydap/fb.h"

/* SSD1306 over 4-wire SPI. The framebuffer is already in SSD1306 page
 * layout, so a frame is sent as-is. */
esp_err_t oled_init(void);
void oled_show(const fb_t *fb);
void oled_set_contrast(uint8_t level);
