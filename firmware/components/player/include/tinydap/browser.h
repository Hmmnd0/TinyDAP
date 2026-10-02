#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * One directory's listing: subfolders first, then audio files, each in
 * natural order ("2 x" before "10 x"). Hidden entries and non-audio files
 * are skipped. Fixed-size storage, no heap use.
 */
#define BROWSER_MAX_ENTRIES 512
#define BROWSER_POOL_BYTES  (24 * 1024)
#define BROWSER_PATH_MAX    256

typedef enum {
    ENTRY_DIR,
    ENTRY_WAV,
    ENTRY_FLAC,
    ENTRY_MP3,
} entry_kind_t;

typedef struct {
    uint16_t name_off;   /* offset into pool */
    uint8_t kind;        /* entry_kind_t */
} browser_entry_t;

typedef struct {
    char path[BROWSER_PATH_MAX];
    int count;
    bool truncated;      /* folder had more entries than fit */
    browser_entry_t entries[BROWSER_MAX_ENTRIES];
    char pool[BROWSER_POOL_BYTES];
} browser_t;

/* Lists `path`. Returns 0, or -1 if it can't be opened (listing left empty). */
int browser_load(browser_t *b, const char *path);

static inline const char *browser_name(const browser_t *b, int i)
{
    return b->pool + b->entries[i].name_off;
}

static inline entry_kind_t browser_kind(const browser_t *b, int i)
{
    return (entry_kind_t)b->entries[i].kind;
}

/* Full path of entry i into out. Returns false if it doesn't fit. */
bool browser_entry_path(const browser_t *b, int i, char *out, int out_size);

/* Index of entry named `name`, or -1. */
int browser_find(const browser_t *b, const char *name);
