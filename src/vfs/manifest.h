#ifndef NUWORLD_VFS_MANIFEST_H
#define NUWORLD_VFS_MANIFEST_H

#include <stddef.h>
#include <stdint.h>

#define NW_MANIFEST_SKIP 1u /* never opened by the game: not hashed or extracted */

#define NW_DISC_DATA_ELF 1u /* the data image is made from its ELF */
#define NW_DISC_ENGLISH 2u  /* English voices only */

typedef struct NwManifestEntry {
    const char *path;   /* uppercase, '/'-separated, no ";1" */
    uint64_t size;
    const char *sha256; /* 64 lowercase hex digits */
    unsigned flags;
} NwManifestEntry;

typedef enum NwRegion { NW_REGION_PAL, NW_REGION_US } NwRegion;

typedef struct NwManifest {
    const NwManifestEntry *entries;
    size_t count;
    const char *name;
    const char *boot;  /* SYSTEM.CNF's BOOT2, VER and VMODE */
    const char *ver;
    const char *vmode;
    NwRegion region;
    unsigned flags;    /* NW_DISC_* */
} NwManifest;

/* From src/vfs/disc_manifest*.inc. */
extern const NwManifest nw_disc_manifest, nw_disc_manifest_usa100, nw_disc_manifest_pal201, nw_disc_manifest_usa101;
/* PAL first, NULL-terminated. */
extern const NwManifest *const nw_disc_manifests[];

/* Case-insensitive. */
const NwManifestEntry *nw_manifest_find(const NwManifest *m, const char *path);

#endif
