#ifndef NUWORLD_VFS_CANON_H
#define NUWORLD_VFS_CANON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vfs/manifest.h"

typedef struct NwCanonFile {
    const NwManifest *disc;
    const char *path;
    const char *sha256;
    const char *result;
    uint32_t size;
    uint32_t seg, segs, word;
    bool scene;
} NwCanonFile;

typedef struct NwCanonTable {
    const NwCanonFile *files;
    size_t count;
    const uint32_t *segs;
    size_t nsegs;
    const uint32_t *words;
    size_t nwords;
} NwCanonTable;

extern const NwCanonTable nw_canon_table;

const NwCanonFile *nw_canon_find(const NwCanonTable *table, const NwManifest *disc, const char *key);
uint8_t *nw_canon_apply(const NwCanonTable *table, const NwCanonFile *file, const uint8_t *raw, size_t n,
                        size_t *size);
uint8_t *nw_canon_load(const NwCanonTable *table, const NwCanonFile *file, const char *path, size_t *size);

#endif
