#ifndef NUWORLD_VFS_DISC_H
#define NUWORLD_VFS_DISC_H

#include <stdbool.h>
#include <stdint.h>

#define NW_DISC_SECTOR 2048 /* user data per sector */

struct SDL_IOStream;

typedef enum NwDiscFormat {
    NW_DISC_COOKED,    /* 2048-byte sectors */
    NW_DISC_RAW_MODE1, /* 2352: data at +16 */
    NW_DISC_RAW_MODE2, /* 2352: Mode 2 Form 1, data at +24 */
} NwDiscFormat;

typedef struct NwDisc {
    struct SDL_IOStream *io;
    NwDiscFormat format;
    uint32_t sector_size;
    uint32_t data_offset;
    uint32_t sectors;
    uint8_t *raw;
} NwDisc;

bool nw_disc_open(NwDisc *disc, const char *path);
void nw_disc_close(NwDisc *disc);

/* Form 2 sectors are refused. */
bool nw_disc_read(NwDisc *disc, uint32_t lsn, uint32_t count, void *dst);

const char *nw_disc_format_name(NwDiscFormat format);

#endif
