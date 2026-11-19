/* SDL errors are per thread, so a job keeps its own error text. */
#ifndef NUWORLD_FRONTEND_IMPORT_H
#define NUWORLD_FRONTEND_IMPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vfs/manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NwImageList {
    char **names; /* file names, sorted case-insensitively */
    size_t count;
} NwImageList;

bool nw_import_is_image(const char *name);

/* .bin files with a .cue of the same name are left out; a missing dir is an empty list. */
bool nw_import_scan(const char *dir, NwImageList *list);
void nw_import_list_free(NwImageList *list);

typedef enum NwImportState {
    NW_IMPORT_RUNNING,
    NW_IMPORT_DONE,
    NW_IMPORT_FAILED,
    NW_IMPORT_CANCELLED,
} NwImportState;

typedef struct NwImport NwImport;

/* known must outlive the job. */
NwImport *nw_import_start(const char *image, const char *root, const NwManifest *const *known);

/* total is 0 while the disc is still being checked. */
NwImportState nw_import_poll(NwImport *job, uint64_t *done, uint64_t *total);

const char *nw_import_error(const NwImport *job);

const NwManifest *nw_import_manifest(const NwImport *job);

void nw_import_cancel(NwImport *job);

/* Cancels a running job and waits for it. */
void nw_import_free(NwImport *job);

#ifdef __cplusplus
}
#endif

#endif
