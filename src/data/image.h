#ifndef NUWORLD_DATA_IMAGE_H
#define NUWORLD_DATA_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "data/image_tables.h"

typedef struct NwImageStats {
    unsigned roots[5]; /* by NW_ROOT_* */
    unsigned objects;
    unsigned regions;
    uint64_t region_bytes;
    unsigned pointers;
    unsigned code_pointers, stub_pointers;
    unsigned vu_tokens;
    unsigned kept_ints;                    /* small values or -1 in pointer slots, kept as integers */
    unsigned unclaimed;                    /* void pointers to retail bytes no object claims (the mirror) */
} NwImageStats;

/* The caller verifies the ELF. */
bool nw_image_load(const NwImageTables *t, const void *elf, size_t size, NwImageStats *stats, char *err,
                   size_t err_size);
/* Another disc (port/data.toml [discs]) from its own boot ELF, while no root is copied or converted. */
bool nw_image_load_disc(const NwImageTables *t, const char *disc, const void *elf, size_t size, NwImageStats *stats,
                        char *err, size_t err_size);

void nw_image_free(void);

void *nw_image_host(uint32_t ps2);
bool nw_image_ps2(const void *host, uint32_t *ps2);
bool nw_image_room(const void *host, size_t *room);

typedef struct NwImageObj {
    const char *name;
    uint32_t addr, size;
    const void *host;
    const struct NwType *type;
    uint32_t count;
} NwImageObj;

void nw_image_each(void (*fn)(const NwImageObj *o, void *user), void *user);

bool nw_image_kept_int(const void *p);

/* mpg as in rndrstream_s.mpg, the address a DMA call tag calls. */
const NwVuProg *nw_vu_prog(const void *mpg);

void nw_image_report(const NwImageStats *s, FILE *out);

/* [0]: what the image made from the ELF, [1]: the C definitions at retail addresses. */
typedef struct NwImageTrip {
    unsigned objects[2];
    unsigned differ[2];
    uint64_t bytes[2];
    unsigned strings;
    unsigned untyped;
} NwImageTrip;

/* Call right after nw_image_load, while the ELF is still in the mirror. */
bool nw_image_roundtrip(NwImageTrip *trip, FILE *out, char *err, size_t err_size);
void nw_image_trip_report(const NwImageTrip *trip, FILE *out);

unsigned nw_image_seed_check(FILE *out);

#endif
