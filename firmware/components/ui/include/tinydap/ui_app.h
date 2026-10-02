#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "tinydap/browser.h"
#include "tinydap/fb.h"
#include "tinydap/player_status.h"

/*
 * Player UI: SD-card browser and Now Playing screen, rendered into the
 * 128x64 framebuffer. Portable: inputs are abstract events (the platform maps
 * keys or buttons onto them) and playback goes through player_ops_t.
 */
typedef enum {
    UI_UP,
    UI_DOWN,
    UI_SELECT,
    UI_BACK,
    UI_PLAY_PAUSE,
    UI_NEXT,
    UI_PREV,
    UI_VOL_UP,
    UI_VOL_DOWN,
    UI_VIEW_TOGGLE,
    UI_REPEAT,          /* toggle folder repeat */
} ui_input_t;

typedef enum {
    VIEW_BROWSER,
    VIEW_NOW_PLAYING,
} ui_view_t;

typedef struct {
    player_ops_t ops;
    char root[BROWSER_PATH_MAX];
    bool storage_ok;

    browser_t browse;       /* folder being viewed */
    int cursor;
    int top;                /* first visible row */

    browser_t queue;        /* folder that's playing */
    int queue_pos;          /* entry index in queue */
    bool queue_valid;
    bool repeat;                /* wrap to the folder's first track */
    uint32_t handled_track_id;  /* last ENDED event acted on */
    uint32_t seen_track_id;     /* last track start seen (incl. gapless) */

    ui_view_t view;
    int volume_db;

    char message[32];
    uint32_t message_until_ms;
} ui_app_t;

void ui_app_init(ui_app_t *app, const char *root, bool storage_ok,
                 const player_ops_t *ops, int volume_db);
void ui_app_input(ui_app_t *app, ui_input_t in, const player_status_t *st, uint32_t now_ms);

/* Call periodically: advances to the next track when one ends. */
void ui_app_tick(ui_app_t *app, const player_status_t *st, uint32_t now_ms);

void ui_app_render(const ui_app_t *app, fb_t *fb, const player_status_t *st, uint32_t now_ms);
