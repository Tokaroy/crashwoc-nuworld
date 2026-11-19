/* Reverb table read from the disc's SYS/LIBSD.IRX at run time: it is Sony's data, so the port carries none of it. */
#ifndef NUWORLD_AUDIO_LIBSD_H
#define NUWORLD_AUDIO_LIBSD_H

#include <stdbool.h>

#include "audio/spu2.h"

struct NwVfs;

bool nw_libsd_presets(const struct NwVfs *vfs, NwSpu2Presets *out);

#endif
