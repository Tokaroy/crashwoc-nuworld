#include "port/disc.h"

#include <SDL3/SDL.h>

#include "shim/shim.h"
#include "vfs/manifest.h"
#include "vfs/vfs.h"


#define NW_DISC_PAL_GLOBAL_SFX 0xC5
#define NW_DISC_ENGLISH_GLOBAL_SFX 177

static const NwManifest *nw_disc(void)
{
    const NwVfs *vfs = nw_shim_file_vfs();
    return vfs ? vfs->manifest : NULL;
}

bool nw_disc_ntsc(void)
{
    const NwManifest *m = nw_disc();
    return m && m->vmode && SDL_strcmp(m->vmode, "NTSC") == 0;
}

int nw_disc_level_sfx(int sfx)
{
    const NwManifest *m = nw_disc();
    if (m && (m->flags & NW_DISC_ENGLISH) && sfx >= NW_DISC_ENGLISH_GLOBAL_SFX) {
        return sfx + NW_DISC_PAL_GLOBAL_SFX - NW_DISC_ENGLISH_GLOBAL_SFX;
    }
    return sfx;
}
