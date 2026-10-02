#include "tinydap/ui_app.h"

#include <stdio.h>
#include <string.h>

#define HEADER_H     9                  /* title row + underline */
#define LIST_Y       10
#define LIST_ROWS    6
#define LIST_TEXT_W  (FB_W - 4)         /* leave room for the scrollbar */
#define VOLUME_MIN   (-60)
#define VOLUME_MAX   0
#define VOLUME_STEP  2
#define MESSAGE_MS   1500
#define RESTART_SECS 3                  /* PREV restarts the track after this */

/* ---------- helpers ---------- */

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s ? s + 1 : path;
}

static void show_message(ui_app_t *app, const char *msg, uint32_t now_ms)
{
    snprintf(app->message, sizeof app->message, "%s", msg);
    app->message_until_ms = now_ms + MESSAGE_MS;
}

static void keep_cursor_visible(ui_app_t *app)
{
    if (app->cursor < app->top) {
        app->top = app->cursor;
    } else if (app->cursor >= app->top + LIST_ROWS) {
        app->top = app->cursor - LIST_ROWS + 1;
    }
}

static bool load_folder(ui_app_t *app, const char *path, uint32_t now_ms)
{
    if (browser_load(&app->browse, path) != 0) {
        show_message(app, "Can't open folder", now_ms);
        return false;
    }
    app->cursor = 0;
    app->top = 0;
    return true;
}

static void play_queue_entry(ui_app_t *app)
{
    char path[BROWSER_PATH_MAX + 128];
    if (browser_entry_path(&app->queue, app->queue_pos, path, sizeof path)) {
        app->ops.play(app->ops.ctx, path);
    }
}

/* Moves to the next/previous WAV in the playing folder. */
static bool play_relative(ui_app_t *app, int dir)
{
    for (int i = app->queue_pos + dir; i >= 0 && i < app->queue.count; i += dir) {
        if (browser_kind(&app->queue, i) == ENTRY_WAV) {
            app->queue_pos = i;
            play_queue_entry(app);
            return true;
        }
    }
    return false;
}

static void play_from_browser(ui_app_t *app, uint32_t now_ms)
{
    entry_kind_t kind = browser_kind(&app->browse, app->cursor);
    if (kind != ENTRY_WAV) {
        show_message(app, "WAV only for now", now_ms);
        return;
    }
    memcpy(&app->queue, &app->browse, sizeof app->queue);
    app->queue_pos = app->cursor;
    app->queue_valid = true;
    play_queue_entry(app);
    app->view = VIEW_NOW_PLAYING;
}

/* ---------- input ---------- */

void ui_app_init(ui_app_t *app, const char *root, bool storage_ok,
                 const player_ops_t *ops, int volume_db)
{
    memset(app, 0, sizeof *app);
    snprintf(app->root, sizeof app->root, "%s", root);
    app->ops = *ops;
    app->storage_ok = storage_ok;
    app->volume_db = volume_db;
    app->view = VIEW_BROWSER;
    if (storage_ok) {
        browser_load(&app->browse, root);
    }
}

static void browser_input(ui_app_t *app, ui_input_t in, uint32_t now_ms)
{
    switch (in) {
    case UI_UP:
        if (app->cursor > 0) {
            app->cursor--;
        }
        break;
    case UI_DOWN:
        if (app->cursor < app->browse.count - 1) {
            app->cursor++;
        }
        break;
    case UI_SELECT:
        if (app->browse.count == 0) {
            break;
        }
        if (browser_kind(&app->browse, app->cursor) == ENTRY_DIR) {
            char path[BROWSER_PATH_MAX + 128];
            if (browser_entry_path(&app->browse, app->cursor, path, sizeof path) &&
                strlen(path) < BROWSER_PATH_MAX) {
                load_folder(app, path, now_ms);
            } else {
                show_message(app, "Path too long", now_ms);
            }
        } else {
            play_from_browser(app, now_ms);
        }
        break;
    case UI_BACK:
        if (strcmp(app->browse.path, app->root) != 0) {
            char child[BROWSER_PATH_MAX];
            char parent[BROWSER_PATH_MAX];
            snprintf(child, sizeof child, "%s", base_name(app->browse.path));
            snprintf(parent, sizeof parent, "%s", app->browse.path);
            *strrchr(parent, '/') = '\0';
            if (load_folder(app, parent, now_ms)) {
                int i = browser_find(&app->browse, child);
                app->cursor = i < 0 ? 0 : i;
            }
        }
        break;
    case UI_VIEW_TOGGLE:
        if (app->queue_valid) {
            app->view = VIEW_NOW_PLAYING;
        }
        break;
    default:
        break;
    }
    keep_cursor_visible(app);
}

void ui_app_input(ui_app_t *app, ui_input_t in, const player_status_t *st, uint32_t now_ms)
{
    switch (in) {
    case UI_PLAY_PAUSE:
        if (st->state == PLAYER_PLAYING || st->state == PLAYER_PAUSED) {
            app->ops.toggle_pause(app->ops.ctx);
        } else if (app->view == VIEW_BROWSER && app->browse.count > 0 &&
                   browser_kind(&app->browse, app->cursor) != ENTRY_DIR) {
            play_from_browser(app, now_ms);
        } else if (app->queue_valid) {
            play_queue_entry(app);
        }
        return;
    case UI_NEXT:
        if (app->queue_valid && !play_relative(app, +1)) {
            show_message(app, "Last track", now_ms);
        }
        return;
    case UI_PREV:
        if (!app->queue_valid) {
            return;
        }
        if (st->fmt.sample_rate &&
            st->elapsed_frames / st->fmt.sample_rate >= RESTART_SECS) {
            play_queue_entry(app);
        } else if (!play_relative(app, -1)) {
            play_queue_entry(app);
        }
        return;
    case UI_VOL_UP:
    case UI_VOL_DOWN: {
        int v = app->volume_db + (in == UI_VOL_UP ? VOLUME_STEP : -VOLUME_STEP);
        v = v < VOLUME_MIN ? VOLUME_MIN : v > VOLUME_MAX ? VOLUME_MAX : v;
        app->volume_db = v;
        app->ops.set_volume_db(app->ops.ctx, v);
        char msg[32];
        snprintf(msg, sizeof msg, "Volume %d dB", v);
        show_message(app, msg, now_ms);
        return;
    }
    default:
        break;
    }

    if (app->view == VIEW_NOW_PLAYING) {
        if (in == UI_BACK || in == UI_VIEW_TOGGLE) {
            app->view = VIEW_BROWSER;
        }
    } else if (app->storage_ok) {
        browser_input(app, in, now_ms);
    }
}

void ui_app_tick(ui_app_t *app, const player_status_t *st, uint32_t now_ms)
{
    (void)now_ms;
    if (st->state == PLAYER_ENDED && app->queue_valid && st->track_id != app->handled_track_id) {
        app->handled_track_id = st->track_id;
        if (!play_relative(app, +1)) {
            app->ops.stop(app->ops.ctx);
        }
    }
}

/* ---------- rendering ---------- */

static void draw_play_icon(fb_t *fb, int x, int y)
{
    for (int c = 0; c < 4; c++) {
        fb_fill_rect(fb, x + c, y + c, 1, 7 - 2 * c, true);
    }
}

static void draw_pause_icon(fb_t *fb, int x, int y)
{
    fb_fill_rect(fb, x, y, 2, 7, true);
    fb_fill_rect(fb, x + 3, y, 2, 7, true);
}

static void draw_state_icon(fb_t *fb, int x, int y, player_state_t s)
{
    if (s == PLAYER_PLAYING) {
        draw_play_icon(fb, x, y);
    } else if (s == PLAYER_PAUSED) {
        draw_pause_icon(fb, x, y);
    }
}

static void draw_header(fb_t *fb, const char *title, const char *right, const player_status_t *st)
{
    int right_w = right ? fb_text_width(right) : 0;
    int icon_w = (st->state == PLAYER_PLAYING || st->state == PLAYER_PAUSED) ? 9 : 0;
    fb_text(fb, 0, 0, title, FB_W - right_w - icon_w - 2, true);
    if (right) {
        fb_text(fb, FB_W - right_w, 0, right, right_w, true);
    }
    draw_state_icon(fb, FB_W - right_w - icon_w, 0, st->state);
    fb_fill_rect(fb, 0, HEADER_H - 1, FB_W, 1, true);
}

static void draw_message(fb_t *fb, const char *msg)
{
    int w = fb_text_width(msg) + 8;
    if (w > FB_W) {
        w = FB_W;
    }
    int x = (FB_W - w) / 2, y = 26;
    fb_fill_rect(fb, x, y, w, 13, false);
    fb_rect(fb, x, y, w, 13, true);
    fb_text(fb, x + 4, y + 3, msg, w - 8, true);
}

static void render_browser(const ui_app_t *app, fb_t *fb, const player_status_t *st)
{
    const browser_t *b = &app->browse;
    bool at_root = strcmp(b->path, app->root) == 0;
    draw_header(fb, at_root ? "SD card" : base_name(b->path), NULL, st);

    if (!app->storage_ok) {
        fb_text(fb, 0, LIST_Y + 8, "No SD card", FB_W, true);
        fb_text(fb, 0, LIST_Y + 18, "Insert a FAT32 card", FB_W, true);
        fb_text(fb, 0, LIST_Y + 28, "and restart.", FB_W, true);
        return;
    }
    if (b->count == 0) {
        fb_text(fb, 0, LIST_Y, "(no folders or audio)", FB_W, true);
        return;
    }

    for (int row = 0; row < LIST_ROWS && app->top + row < b->count; row++) {
        int i = app->top + row;
        int y = LIST_Y + row * FONT_H;
        char label[BROWSER_PATH_MAX];
        snprintf(label, sizeof label, browser_kind(b, i) == ENTRY_DIR ? "%s/" : "%s",
                 browser_name(b, i));
        fb_text(fb, 1, y, label, LIST_TEXT_W - 1, true);
        if (i == app->cursor) {
            fb_invert_rect(fb, 0, y - 1, LIST_TEXT_W, FONT_H);
        }
    }

    if (b->count > LIST_ROWS) {
        int track_h = LIST_ROWS * FONT_H;
        int thumb_h = track_h * LIST_ROWS / b->count;
        if (thumb_h < 3) {
            thumb_h = 3;
        }
        int thumb_y = LIST_Y - 1 + (track_h - thumb_h) * app->top / (b->count - LIST_ROWS);
        fb_fill_rect(fb, FB_W - 2, thumb_y, 2, thumb_h, true);
    }
}

static void format_time(char *out, size_t n, uint32_t secs)
{
    if (secs >= 3600) {
        snprintf(out, n, "%u:%02u:%02u", (unsigned)(secs / 3600), (unsigned)(secs / 60 % 60),
                 (unsigned)(secs % 60));
    } else {
        snprintf(out, n, "%u:%02u", (unsigned)(secs / 60), (unsigned)(secs % 60));
    }
}

static void render_now_playing(const ui_app_t *app, fb_t *fb, const player_status_t *st)
{
    char right[24] = "";
    if (app->queue_valid) {
        int pos = 0, total = 0;
        for (int i = 0; i < app->queue.count; i++) {
            if (browser_kind(&app->queue, i) == ENTRY_WAV) {
                total++;
                if (i <= app->queue_pos) {
                    pos = total;
                }
            }
        }
        snprintf(right, sizeof right, "%d/%d", pos, total);
    }
    draw_header(fb, "Now Playing", right, st);

    if (st->state == PLAYER_STOPPED || st->path[0] == '\0') {
        fb_text(fb, 0, 20, "Nothing playing", FB_W, true);
        return;
    }

    /* Title: file name without extension, wrapped over two lines. */
    char title[BROWSER_PATH_MAX];
    snprintf(title, sizeof title, "%s", base_name(st->path));
    char *dot = strrchr(title, '.');
    if (dot) {
        *dot = '\0';
    }
    int per_line = FB_W / FONT_W;
    fb_text(fb, 0, 12, title, FB_W, true);
    if ((int)strlen(title) > per_line) {
        fb_text(fb, 0, 21, title + per_line, FB_W, true);
    }

    if (st->state == PLAYER_ERROR) {
        fb_text(fb, 0, 32, st->error, FB_W, true);
        return;
    }

    /* Folder, as a stand-in for album until tags are parsed. */
    char folder[BROWSER_PATH_MAX];
    snprintf(folder, sizeof folder, "%s", st->path);
    char *slash = strrchr(folder, '/');
    if (slash) {
        *slash = '\0';
    }
    fb_text(fb, 0, 31, base_name(folder), FB_W, true);

    char line[64];
    unsigned rate = (unsigned)st->fmt.sample_rate;
    if (rate % 1000 == 0) {
        snprintf(line, sizeof line, "%ukHz %ubit WAV", rate / 1000, st->fmt.bits_per_sample);
    } else {
        snprintf(line, sizeof line, "%u.%ukHz %ubit WAV", rate / 1000, rate % 1000 / 100,
                 st->fmt.bits_per_sample);
    }
    fb_text(fb, 0, 40, line, FB_W, true);

    fb_rect(fb, 0, 49, FB_W, 5, true);
    if (st->total_frames) {
        int w = (int)((uint64_t)(FB_W - 4) * st->elapsed_frames / st->total_frames);
        fb_fill_rect(fb, 2, 51, w, 1, true);
    }

    char elapsed[24], total[24];
    uint32_t rate_or_1 = st->fmt.sample_rate ? st->fmt.sample_rate : 1;
    format_time(elapsed, sizeof elapsed, st->elapsed_frames / rate_or_1);
    format_time(total, sizeof total, st->total_frames / rate_or_1);
    snprintf(line, sizeof line, "%s/%s", elapsed, total);
    fb_text(fb, 0, 57, line, FB_W, true);

    char vol[16];
    snprintf(vol, sizeof vol, "%ddB", app->volume_db);
    fb_text(fb, FB_W - fb_text_width(vol), 57, vol, FB_W, true);
}

void ui_app_render(const ui_app_t *app, fb_t *fb, const player_status_t *st, uint32_t now_ms)
{
    fb_clear(fb);
    if (app->view == VIEW_NOW_PLAYING) {
        render_now_playing(app, fb, st);
    } else {
        render_browser(app, fb, st);
    }
    if (app->message[0] && (int32_t)(app->message_until_ms - now_ms) > 0) {
        draw_message(fb, app->message);
    }
}
