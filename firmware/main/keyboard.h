#pragma once

#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Cardputer-Adv keyboard: TCA8418 matrix controller, polled over I2C. */

/* Non-printing keys */
#define KEY_BACKSPACE 0x08
#define KEY_TAB       0x09
#define KEY_ENTER     '\n'
#define KEY_FN        0x11
#define KEY_SHIFT     0x12
#define KEY_CTRL      0x13
#define KEY_OPT       0x14
#define KEY_ALT       0x15

typedef struct {
    char key;       /* unshifted legend, or one of the KEY_ codes above */
    bool pressed;   /* false on release */
} key_event_t;

esp_err_t keyboard_init(i2c_master_bus_handle_t bus, uint8_t addr);

/* Reads up to max queued key events. Returns how many were read. */
int keyboard_read(key_event_t *ev, int max);
