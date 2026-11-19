#include "port/game.h"
#include "shim/shim.h"

/* Returns the field it starts, alternating as an interlaced display's do. */
int sceGsSyncV(int mode)
{
    (void)mode;
    return (int)(nw_game_yield(true) & 1);
}
