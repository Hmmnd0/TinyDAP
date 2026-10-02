#pragma once

#include <stdbool.h>

#define SDCARD_MOUNT "/sdcard"

/* Mounts the microSD card (SPI mode, FAT32) at SDCARD_MOUNT. */
bool sdcard_mount(void);
