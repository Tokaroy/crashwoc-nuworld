#include <stdio.h>

#include "port/disc.h"
#include "shim/shim.h"
#include "vfs/manifest.h"
#include "vfs/vfs.h"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

int main(void)
{
    NwVfs vfs = { .manifest = &nw_disc_manifest_usa101 };
    nw_shim_file_init(&vfs);
    CHECK(nw_disc() == &nw_disc_manifest_usa101 && nw_disc_region() == NW_REGION_US);
    vfs.manifest = &nw_disc_manifest_usa100;
    CHECK(nw_disc_region() == NW_REGION_US);
    vfs.manifest = &nw_disc_manifest_pal201;
    CHECK(nw_disc() == &nw_disc_manifest_pal201 && nw_disc_region() == NW_REGION_PAL);
    vfs.manifest = &nw_disc_manifest;
    CHECK(nw_disc_region() == NW_REGION_PAL);
    nw_shim_file_init(NULL);
    CHECK(!nw_disc() && nw_disc_region() == NW_REGION_PAL);

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
