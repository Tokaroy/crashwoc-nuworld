/* No includes or typedef'd integers: every decomp wrapper ends with a .data.inc, after its own declarations. */
#ifndef NUWORLD_DATA_IMAGE_TABLES_H
#define NUWORLD_DATA_IMAGE_TABLES_H

struct NwType;

/* NwImageRoot.kind */
enum {
    NW_ROOT_MIRROR,  /* plain data at its PS2 offset in the mirror of the retail image */
    NW_ROOT_COPY,    /* plain data the host declaration needs aligned better than its PS2 address */
    NW_ROOT_CONVERT,
    NW_ROOT_ALIAS,   /* inside another root or a C-defined object (no storage of its own) */
    NW_ROOT_VU
};

typedef struct NwImageRoot {
    const char *name;
    unsigned int addr;
    unsigned int ps2_size; /* 0: unsized (mirror only) */
    void *host;
    unsigned int host_size;
    const struct NwType *elem;
    unsigned int count;
    unsigned int kind;
} NwImageRoot;

#define NW_DATA_PLAIN 1u
#define NW_DATA_RETAIL 2u /* a placeholder for retail's bytes (status state "retail"): copied in */

typedef struct NwDataObj {
    const char *name;
    unsigned int addr;
    unsigned int ps2_size;
    void *host;
    unsigned int host_size;
    const struct NwType *type; /* element type; NULL: plain, or only offset 0 maps */
    unsigned int count;
    unsigned int flags;
} NwDataObj;

typedef struct NwDataUnit {
    const char *unit;
    const NwDataObj *objs;
    unsigned int count;
} NwDataUnit;

/* The EE only takes a VU1 package's address (mtl->rs.mpg) for DMA tags to call; on the host it is this token. */
typedef struct NwVuProg {
    const char *name;
    unsigned int addr, size;
} NwVuProg;

/* port/data.toml [seeds] */
typedef struct NwImageSeed {
    const char *name;
    unsigned int addr, size;
    const unsigned char *bytes;
} NwImageSeed;

typedef struct NwImageMove {
    const char *name;
    unsigned int addr, at; /* in the PAL ELF, in the disc's */
} NwImageMove;

/* port/data.toml [discs] */
typedef struct NwImageDisc {
    const char *name;
    const NwImageMove *moves;
    unsigned int nmoves;
} NwImageDisc;

typedef struct NwImageTables {
    unsigned int mirror_base, mirror_size;
    unsigned char *mirror;
    unsigned int text_base, text_end;
    unsigned int vutext_base, vutext_end;
    const NwVuProg *const *vu_progs;
    unsigned int nvu_progs;
    const NwImageRoot *roots;
    unsigned int nroots;
    const NwDataUnit *const *units;
    unsigned int nunits;
    const unsigned int (*registry)[2]; /* sized retail data symbols {address, size}, address order */
    unsigned int nregistry;
    const unsigned int *bounds; /* every known object start, range and section boundary */
    unsigned int nbounds;
    const NwImageSeed *seeds;
    unsigned int nseeds;
    const NwImageDisc *discs;
    unsigned int ndiscs;
} NwImageTables;

extern const NwImageTables nw_image_tables;

#endif
