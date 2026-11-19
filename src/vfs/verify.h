#ifndef NUWORLD_VFS_VERIFY_H
#define NUWORLD_VFS_VERIFY_H

#include <stdbool.h>
#include <stdint.h>

#include "vfs/disc.h"
#include "vfs/iso9660.h"
#include "vfs/manifest.h"
#include "vfs/sha256.h"

#define NW_VERIFY_SKIPPED 1u /* also hash NW_MANIFEST_SKIP entries */

/* Returning false cancels the operation with the error "cancelled". */
typedef bool (*NwProgressFn)(void *user, uint64_t done, uint64_t total);

/* The disc of known that SYSTEM.CNF names, once it and the boot ELF match its manifest. */
const NwManifest *nw_verify_identity(NwDisc *disc, const NwIso *iso, const NwManifest *const *known);

/* Names and sizes only, no hashing. */
bool nw_verify_listing(const NwIso *iso, const NwManifest *m);

bool nw_verify_files(NwDisc *disc, const NwIso *iso, const NwManifest *m, unsigned flags, NwProgressFn progress,
                     void *user);

bool nw_verify_digest(const NwManifestEntry *e, const uint8_t digest[NW_SHA256_SIZE]);

#endif
