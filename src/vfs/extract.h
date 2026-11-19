#ifndef NUWORLD_VFS_EXTRACT_H
#define NUWORLD_VFS_EXTRACT_H

#include <stdbool.h>

#include "vfs/disc.h"
#include "vfs/iso9660.h"
#include "vfs/manifest.h"
#include "vfs/verify.h"

#define NW_DISC_DIR "disc"
#define NW_DISC_STAMP ".nuworld-disc"

/* An existing disc/ is only replaced on success; a failure leaves no disc.partial/. */
const NwManifest *nw_extract_disc(NwDisc *disc, const NwIso *iso, const NwManifest *const *known, const char *root,
                                  NwProgressFn progress, void *user);

/* Only checks the stamp; nothing is hashed. */
bool nw_extract_has_disc(const char *root);

/* The disc the stamp names; an old stamp without a name is known[0], PAL. */
const NwManifest *nw_extract_manifest(const char *root, const NwManifest *const *known);

bool nw_extract_verify(const char *root, const NwManifest *m, NwProgressFn progress, void *user);

#endif
