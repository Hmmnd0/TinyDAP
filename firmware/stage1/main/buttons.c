#include "buttons.h"

#include <stdint.h>

#include "board_devkitc.h"
#include "driver/gpio.h"

/* Polled every 5 ms. Cheap tactile switches chatter for tens of ms on both
 * press and release, so a state must hold for 40 ms to count. */
#define DEBOUNCE_POLLS 8

static const int s_gpio[BTN_COUNT] = {
    BOARD_BTN_PLAY, BOARD_BTN_PREV, BOARD_BTN_NEXT, BOARD_BTN_VOLUP, BOARD_BTN_VOLDN,
};
static const char *s_name[BTN_COUNT] = { "Play", "Prev", "Next", "Vol+", "Vol-" };
static bool s_down[BTN_COUNT];
static uint8_t s_count[BTN_COUNT];

void buttons_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < BTN_COUNT; i++) {
        mask |= 1ULL << s_gpio[i];
    }
    gpio_config_t io = { .pin_bit_mask = mask, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&io);
}

button_t buttons_poll(void)
{
    button_t pressed = BTN_COUNT;
    for (int i = 0; i < BTN_COUNT; i++) {
        bool down = gpio_get_level(s_gpio[i]) == 0;
        if (down == s_down[i]) {
            s_count[i] = 0;
            continue;
        }
        if (++s_count[i] >= DEBOUNCE_POLLS) {
            s_down[i] = down;
            s_count[i] = 0;
            if (down && pressed == BTN_COUNT) {
                pressed = (button_t)i;
            }
        }
    }
    return pressed;
}

bool button_is_down(button_t b) { return b < BTN_COUNT && s_down[b]; }
const char *button_name(button_t b) { return b < BTN_COUNT ? s_name[b] : "-"; }
int button_gpio(button_t b) { return b < BTN_COUNT ? s_gpio[b] : -1; }
