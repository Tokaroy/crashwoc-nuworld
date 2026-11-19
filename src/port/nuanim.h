#ifndef NUWORLD_PORT_NUANIM_H
#define NUWORLD_PORT_NUANIM_H

#include <stddef.h>

/* at(): the size bytes a pointer field addresses (NULL for 0); alloc(): zeroed host records of retail's at ps2. */
typedef struct NwAnimFile {
    char *(*at)(struct NwAnimFile *f, char *field, size_t size);
    void *(*alloc)(struct NwAnimFile *f, size_t size, size_t align, const void *ps2);
    int node_names; /* relocated (the .ani), or the file's value (the animlib) */
} NwAnimFile;

/* Retail's NuAnimDataFixPtrs walk from the record at p: chunk keys and curves keep the file's values. */
struct nuanimdata_s *nw_anim_data(NwAnimFile *f, char *p);

#endif
