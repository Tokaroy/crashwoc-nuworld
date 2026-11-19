/* Levels and hubs are keyed by a registry, not by position; encoding round-trips byte for byte. */
#ifndef NUWORLD_SAVE_SAVECODEC_H
#define NUWORLD_SAVE_SAVECODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "save/savedoc.h"

#define NW_SAVE_FORMAT 1 /* documents with a higher format are refused */

/* struct game_s (decomp/include/game/gametypes.h); src/port/saves.c checks these against it. */
#define NW_SAVE_GAME_SIZE 0x40C
#define NW_SAVE_NAME 0x000
#define NW_SAVE_NAME_SIZE 9
#define NW_SAVE_VIBRATION 0x009
#define NW_SAVE_SURROUND 0x00A
#define NW_SAVE_SFX_VOLUME 0x00B
#define NW_SAVE_MUSIC_VOLUME 0x00C
#define NW_SAVE_SCREEN_X 0x00D
#define NW_SAVE_SCREEN_Y 0x00E
#define NW_SAVE_LANGUAGE 0x00F
#define NW_SAVE_HUB 0x010
#define NW_SAVE_HUBS 6
#define NW_SAVE_LEVEL 0x028
#define NW_SAVE_LEVELS 35
#define NW_SAVE_LIVES 0x3FC
#define NW_SAVE_WUMPAS 0x3FD
#define NW_SAVE_MASK 0x3FE
#define NW_SAVE_PERCENT 0x3FF
#define NW_SAVE_CRYSTALS 0x400
#define NW_SAVE_RELICS 0x401
#define NW_SAVE_CRATE_GEMS 0x402
#define NW_SAVE_BONUS_GEMS 0x403
#define NW_SAVE_GEMS 0x404
#define NW_SAVE_GEMBITS 0x405
#define NW_SAVE_POWERBITS 0x406
#define NW_SAVE_EMPTY 0x407
#define NW_SAVE_CUTBITS 0x408
#define NW_SAVE_HUB_SIZE 4
#define NW_SAVE_HUB_FLAGS 0x0
#define NW_SAVE_HUB_CRYSTALS 0x1
#define NW_SAVE_HUB_PAD 0x2
/* The loading minigame's best lives in the top floor hub's padding, which retail never touches. */
#define NW_SAVE_MINIGAME_HUB 5
#define NW_SAVE_MINIGAME_BEST (NW_SAVE_HUB + NW_SAVE_MINIGAME_HUB * NW_SAVE_HUB_SIZE + NW_SAVE_HUB_PAD)
#define NW_SAVE_LEVEL_SIZE 0x1C
#define NW_SAVE_LEVEL_FLAGS 0x00
#define NW_SAVE_LEVEL_PAD 0x02
#define NW_SAVE_LEVEL_TIME 0x04
#define NW_SAVE_TIMES 3
#define NW_SAVE_TIME_SIZE 8
#define NW_SAVE_TIME_NAME 0x0
#define NW_SAVE_TIME_NAME_SIZE 4
#define NW_SAVE_TIME_ITIME 0x4
#define NW_SAVE_LANGUAGES 6
#define NW_SAVE_VOLUME_MAX 100

/* Longer names beside a record's own (port/names.h); empty when the record's own name stands. */
#define NW_SAVE_LONG_NAME 16
#define NW_SAVE_LONG_TIME 8
typedef struct NwSaveNames {
    char game[NW_SAVE_LONG_NAME + 1];
    char time[NW_SAVE_LEVELS][NW_SAVE_TIMES][NW_SAVE_LONG_TIME + 1];
} NwSaveNames;

typedef struct NwSaveRegistry {
    char **levels;
    size_t level_count;
    char **hubs;
    size_t hub_count;
} NwSaveRegistry;

/* "a\\cold_a\\cold_a" -> "a/cold_a/cold_a"; a missing, empty or repeated path gets "#<index>". */
bool nw_save_registry_init(NwSaveRegistry *reg, const char *const *paths, size_t levels, size_t hubs);
void nw_save_registry_free(NwSaveRegistry *reg);

int64_t nw_save_format(const NwDoc *doc);
bool nw_save_merge(NwDoc *doc, const NwSaveRegistry *reg, const uint8_t *games, size_t count);
/* A missing slot is empty, a missing field zero. */
bool nw_save_decode(const NwDoc *doc, const NwSaveRegistry *reg, uint8_t *games, size_t count);
/* Texts end in a NUL, options in the menus' ranges; a PS2 save's games need it before they are merged. */
bool nw_save_check(const NwSaveRegistry *reg, const uint8_t *games, size_t count);

/* After nw_save_merge wrote the record; an empty name drops its key. */
bool nw_save_names_merge(NwDoc *doc, const NwSaveRegistry *reg, size_t index, const NwSaveNames *names);
/* A name that is missing, too long or holds anything but letters, digits and spaces is empty. */
void nw_save_names_decode(const NwDoc *doc, const NwSaveRegistry *reg, size_t index, NwSaveNames *names);
bool nw_save_name_ok(const char *text, size_t len, size_t max);

#endif
