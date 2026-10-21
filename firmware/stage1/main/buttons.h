#pragma once

#include <stdbool.h>

typedef enum { BTN_PLAY, BTN_PREV, BTN_NEXT, BTN_VOLUP, BTN_VOLDN, BTN_COUNT } button_t;

void buttons_init(void);

/* Call every few ms. Returns a button that was just pressed (debounced),
 * or BTN_COUNT if none. */
button_t buttons_poll(void);

/* Debounced state, for showing which buttons are held. */
bool button_is_down(button_t b);

const char *button_name(button_t b);
int button_gpio(button_t b);
