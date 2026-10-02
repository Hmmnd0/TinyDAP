#pragma once

#include <stdint.h>

#include "tinydap/audio_sink.h"

typedef enum {
    PLAYER_STOPPED,
    PLAYER_PLAYING,
    PLAYER_PAUSED,
    PLAYER_ENDED,    /* reached end of track; UI decides what's next */
    PLAYER_ERROR,
} player_state_t;

/* Snapshot of playback state for the UI. */
typedef struct {
    player_state_t state;
    uint32_t track_id;           /* increments every time a track starts */
    char path[256];
    char error[32];
    audio_format_t fmt;          /* source file format */
    uint32_t elapsed_frames;
    uint32_t total_frames;
    uint32_t underruns;
    uint8_t buffer_pct;
} player_status_t;

/* Commands the UI sends to the player. Implemented per platform. */
typedef struct {
    void (*play)(void *ctx, const char *path);
    void (*toggle_pause)(void *ctx);
    void (*stop)(void *ctx);
    void (*set_volume_db)(void *ctx, int db);
    void *ctx;
} player_ops_t;
