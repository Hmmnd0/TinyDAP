#include "tinydap/browser.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static int kind_for_name(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot) {
        return -1;
    }
    if (strcasecmp(dot, ".wav") == 0) return ENTRY_WAV;
    if (strcasecmp(dot, ".flac") == 0) return ENTRY_FLAC;
    if (strcasecmp(dot, ".mp3") == 0) return ENTRY_MP3;
    return -1;
}

/* Case-insensitive compare that orders digit runs by numeric value. */
static int natural_cmp(const char *a, const char *b)
{
    while (*a && *b) {
        if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
            while (*a == '0') a++;
            while (*b == '0') b++;
            const char *da = a, *db = b;
            while (isdigit((unsigned char)*a)) a++;
            while (isdigit((unsigned char)*b)) b++;
            long la = a - da, lb = b - db;
            if (la != lb) {
                return la < lb ? -1 : 1;
            }
            int c = strncmp(da, db, (size_t)la);
            if (c) {
                return c;
            }
        } else {
            int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
            if (ca != cb) {
                return ca - cb;
            }
            a++;
            b++;
        }
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static const char *s_sort_pool;  /* qsort has no context argument */

static int entry_cmp(const void *pa, const void *pb)
{
    const browser_entry_t *a = pa, *b = pb;
    bool da = a->kind == ENTRY_DIR, db = b->kind == ENTRY_DIR;
    if (da != db) {
        return da ? -1 : 1;
    }
    return natural_cmp(s_sort_pool + a->name_off, s_sort_pool + b->name_off);
}

int browser_load(browser_t *b, const char *path)
{
    snprintf(b->path, sizeof b->path, "%s", path);
    b->count = 0;
    b->truncated = false;

    DIR *dir = opendir(path);
    if (!dir) {
        return -1;
    }

    size_t used = 0;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        const char *name = de->d_name;
        if (name[0] == '.' || strcmp(name, "System Volume Information") == 0) {
            continue;
        }

        bool is_dir;
        if (de->d_type == DT_DIR) {
            is_dir = true;
        } else if (de->d_type == DT_UNKNOWN) {
            char full[BROWSER_PATH_MAX + 64];
            struct stat st;
            snprintf(full, sizeof full, "%s/%s", path, name);
            is_dir = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
        } else {
            is_dir = false;
        }
        int kind = is_dir ? ENTRY_DIR : kind_for_name(name);
        if (kind < 0) {
            continue;
        }

        size_t len = strlen(name) + 1;
        if (b->count >= BROWSER_MAX_ENTRIES || used + len > sizeof b->pool) {
            b->truncated = true;
            break;
        }
        memcpy(b->pool + used, name, len);
        b->entries[b->count].name_off = (uint16_t)used;
        b->entries[b->count].kind = (uint8_t)kind;
        b->count++;
        used += len;
    }
    closedir(dir);

    s_sort_pool = b->pool;
    qsort(b->entries, (size_t)b->count, sizeof b->entries[0], entry_cmp);
    return 0;
}

bool browser_entry_path(const browser_t *b, int i, char *out, int out_size)
{
    int n = snprintf(out, (size_t)out_size, "%s/%s", b->path, browser_name(b, i));
    return n > 0 && n < out_size;
}

int browser_find(const browser_t *b, const char *name)
{
    for (int i = 0; i < b->count; i++) {
        if (strcmp(browser_name(b, i), name) == 0) {
            return i;
        }
    }
    return -1;
}
