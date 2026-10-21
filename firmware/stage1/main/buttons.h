#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum { BTN_PLAY, BTN_PREV, BTN_NEXT, BTN_VOLUP, BTN_VOLDN, BTN_COUNT } button_t;

typedef enum {
    BTN_EV_NONE,
    BTN_EV_SHORT,   /* released before the long-press time */
    BTN_EV_LONG,    /* held for the long-press time (fires once, while held) */
    BTN_EV_REPEAT,  /* pressed, then repeating while held (repeat buttons only) */
} button_ev_t;

typedef struct {
    button_t button;
    button_ev_t ev;
} button_event_t;

void buttons_init(void);

/* Call every BUTTONS_POLL_MS. Returns one event per call at most. Vol+/Vol-
 * repeat while held; the others report short or long presses. */
#define BUTTONS_POLL_MS 5
button_event_t buttons_poll(uint32_t now_ms);

bool button_is_down(button_t b);
const char *button_name(button_t b);
int button_gpio(button_t b);
