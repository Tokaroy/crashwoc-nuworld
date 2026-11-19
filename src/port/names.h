/* A longer name stands while the game's own name is its start, so a name the game sets itself wins. Game thread. */
#ifndef NUWORLD_PORT_NAMES_H
#define NUWORLD_PORT_NAMES_H

#include <stdbool.h>
#include <stddef.h>

#include "save/savecodec.h"

#define NW_NAMES_GAME NW_SAVE_LONG_NAME
#define NW_NAMES_OWN_GAME 8             /* Game.name's letters */
#define NW_NAMES_TIME NW_SAVE_LONG_TIME
#define NW_NAMES_OWN_TIME 3

struct game_s;

/* nw_names_slot(i) is SaveSlot[i / 2][i % 2]'s. */
NwSaveNames *nw_names_game(void);
NwSaveNames *nw_names_slot(int i);
void nw_names_save(int i);
void nw_names_load(int i);

int nw_names_time_max(void);

/* g is Game or a SaveSlot. */
char *nw_names_of(const struct game_s *g);
/* time: a struct time_s of Game or a SaveSlot (NW_TIME_NAME in nu_port.h). */
char *nw_names_time_text(const void *time);
/* scale for text that has more than fit letters, so it keeps the width fit letters have at scale. */
float nw_names_scale(float scale, const char *text, size_t fit);

#endif
