#ifndef NUWORLD_FRONTEND_SAVEEXPORT_H
#define NUWORLD_FRONTEND_SAVEEXPORT_H

#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "vfs/vfs.h"

/* STUFF/CRASH.ICO, SDL_malloc'd; NULL with an SDL error. */
void *nw_saveexport_icon(const NwVfs *vfs, size_t *len);
const char *nw_saveexport_ps2_name(const NwVfs *vfs);
char *nw_saveexport_cards_folder(void);
void nw_saveexport_game(const uint8_t *game, char *out, size_t size);

/* dir ends in a separator and is copied. */
void nw_saveexport_start(SDL_Window *window, const char *dir, const NwVfs *vfs);
void nw_saveexport_stop(void);

#endif
