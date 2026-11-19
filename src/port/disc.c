#include "port/disc.h"

#include "shim/shim.h"
#include "vfs/manifest.h"
#include "vfs/vfs.h"

const NwManifest *nw_disc(void)
{
    const NwVfs *vfs = nw_shim_file_vfs();
    return vfs ? vfs->manifest : NULL;
}

NwRegion nw_disc_region(void)
{
    const NwManifest *m = nw_disc();
    return m ? m->region : NW_REGION_PAL;
}
