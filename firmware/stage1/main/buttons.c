#include "buttons.h"

#include "board_devkitc.h"
#include "driver/gpio.h"

/* Cheap tactile switches chatter for tens of ms on both press and release,
 * so a state must hold for 40 ms to count (15 ms let presses count twice). */
#define DEBOUNCE_POLLS  8
#define LONG_MS         600
#define REPEAT_DELAY_MS 400
#define REPEAT_RATE_MS  120

static const int s_gpio[BTN_COUNT] = {
    BOARD_BTN_PLAY, BOARD_BTN_PREV, BOARD_BTN_NEXT, BOARD_BTN_VOLUP, BOARD_BTN_VOLDN,
};
static const char *s_name[BTN_COUNT] = { "Play", "Prev", "Next", "Vol+", "Vol-" };

static struct {
    bool down;
    uint8_t count;
    uint32_t since;      /* ms when it went down */
    uint32_t next_rep;
    bool long_sent;
} s_b[BTN_COUNT];

static bool repeats(button_t b) { return b == BTN_VOLUP || b == BTN_VOLDN; }

void buttons_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < BTN_COUNT; i++) {
        mask |= 1ULL << s_gpio[i];
    }
    gpio_config_t io = { .pin_bit_mask = mask, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    gpio_config(&io);
}

button_event_t buttons_poll(uint32_t now)
{
    button_event_t out = { BTN_COUNT, BTN_EV_NONE };
    for (int i = 0; i < BTN_COUNT; i++) {
        bool down = gpio_get_level(s_gpio[i]) == 0;
        bool changed = false;
        if (down != s_b[i].down) {
            if (++s_b[i].count >= DEBOUNCE_POLLS) {
                s_b[i].down = down;
                s_b[i].count = 0;
                changed = true;
            }
        } else {
            s_b[i].count = 0;
        }
        if (out.ev != BTN_EV_NONE) {
            continue;
        }
        button_t b = (button_t)i;
        if (changed && down) {
            s_b[i].since = now;
            s_b[i].long_sent = false;
            if (repeats(b)) {
                s_b[i].next_rep = now + REPEAT_DELAY_MS;
                out = (button_event_t){ b, BTN_EV_REPEAT };
            }
        } else if (changed && !down) {
            if (!repeats(b) && !s_b[i].long_sent) {
                out = (button_event_t){ b, BTN_EV_SHORT };
            }
        } else if (s_b[i].down) {
            if (repeats(b) && (int32_t)(now - s_b[i].next_rep) >= 0) {
                s_b[i].next_rep = now + REPEAT_RATE_MS;
                out = (button_event_t){ b, BTN_EV_REPEAT };
            } else if (!repeats(b) && !s_b[i].long_sent && now - s_b[i].since >= LONG_MS) {
                s_b[i].long_sent = true;
                out = (button_event_t){ b, BTN_EV_LONG };
            }
        }
    }
    return out;
}

bool button_is_down(button_t b) { return b < BTN_COUNT && s_b[b].down; }
const char *button_name(button_t b) { return b < BTN_COUNT ? s_name[b] : "-"; }
int button_gpio(button_t b) { return b < BTN_COUNT ? s_gpio[b] : -1; }
