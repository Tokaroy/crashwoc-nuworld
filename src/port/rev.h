#ifndef NUWORLD_PORT_REV_H
#define NUWORLD_PORT_REV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern int nw_rev;

typedef struct NwRevValue {
    const char *name;
    uint32_t addr;
    int size;
    uint32_t old, pal, ntsc;
} NwRevValue;

bool nw_rev_set(const char *name);
const char *nw_rev_name(void);
int nw_rev_apply(const NwRevValue *values, size_t count, int hz, void *(*host)(uint32_t addr));
int nw_rev_patch(int hz);

#endif
