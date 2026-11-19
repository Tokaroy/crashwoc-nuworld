#include "port/rev.h"

#include <string.h>

#include <SDL3/SDL.h>

#include "data/image.h"

int nw_rev = 1;

extern char *LevelName[44][6];

static const NwRevValue nw_rev_values[] = {
    { "CData+0x578", 0x005601d0u, 4, 0x3F0CCCCDu, 0x3F59999Au, 0x3F59999Au },
    { "AkuAkuAnim+0xc", 0x0055d264u, 4, 0xFFFF0006u, 0xFFFF0007u, 0xFFFF0007u },
    { "AkuAkuAnim+0x10", 0x0055d268u, 4, 0x00000080u, 0xFFFFFFFFu, 0xFFFFFFFFu },
    { "AkuAkuAnim+0x14", 0x0055d26cu, 4, 0x00000000u, 0xFFFFFFFFu, 0xFFFFFFFFu },
    { "ArcticAnticsModelList+0x4", 0x00586884u, 4, 0x034F8C0Au, 0x038C4F0Au, 0x038C4F0Au },
    { "SfxTabGLOBAL+0xa96", 0x005e6796u, 1, 0u, 10u, 12u },
};

static const char *const nw_rev_warp_room[6] = { "WARP ROOM", "SAS VIRTUEL", "ZEITSPRUNG-RAUM", "STANZA TELETR.",
                                                 "SALA DEL TIEMPO", "ZAPKAMER" };

bool nw_rev_set(const char *name)
{
    bool late = SDL_strcmp(name, "late") == 0;
    if (!late && SDL_strcmp(name, "1.03") != 0) {
        return false;
    }
    nw_rev = late;
    return true;
}

const char *nw_rev_name(void)
{
    return nw_rev ? "late" : "1.03";
}

int nw_rev_apply(const NwRevValue *values, size_t count, int hz, void *(*host)(uint32_t addr))
{
    int kept = 0;
    for (size_t i = 0; i < count; i++) {
        const NwRevValue *v = &values[i];
        unsigned char old[4], late[4];
        uint32_t value = hz == 60 ? v->ntsc : v->pal;
        for (int k = 0; k < v->size; k++) {
            old[k] = (unsigned char)(v->old >> 8 * k);
            late[k] = (unsigned char)(value >> 8 * k);
        }
        unsigned char *at = host(v->addr);
        if (!at || memcmp(at, old, (size_t)v->size) != 0) {
            kept++;
            continue;
        }
        memcpy(at, late, (size_t)v->size);
    }
    return kept;
}

int nw_rev_patch(int hz)
{
    for (int i = 0; i < 6; i++) {
        LevelName[37][i] = (char *)nw_rev_warp_room[i];
    }
    return nw_rev_apply(nw_rev_values, SDL_arraysize(nw_rev_values), hz, nw_image_host);
}
