#pragma once

#include <stdbool.h>

#define SDCARD_MOUNT "/sdcard"

/* Mounts the microSD over 4-bit SDMMC at SDCARD_MOUNT. Tries high speed
 * (40 MHz) first, then default speed (20 MHz), then 1-bit. */
bool sdcard_mount(void);
