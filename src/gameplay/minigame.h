/* Cut from every retail build (MiniGame* in game/main are empty), rebuilt on retail's leftover mg_* tuning. */
#ifndef NUWORLD_GAMEPLAY_MINIGAME_H
#define NUWORLD_GAMEPLAY_MINIGAME_H

#include <stdbool.h>

/* UI thread: reloads the current level, or another Crash or Coco level with fruit. */
void nw_minigame_start(void);
/* save/savecodec.h NW_SAVE_MINIGAME_BEST; the game saves it with the rest. */
int nw_minigame_best(void);
/* From GO! until the last fruit: BGLoad's LOADING message is hidden. */
bool nw_minigame_playing(void);

#endif
