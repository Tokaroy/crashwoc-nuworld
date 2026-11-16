#include "port/rev.h"

#include <SDL3/SDL.h>

int nw_rev;

bool nw_rev_set(const char *name)
{
    bool late = SDL_strcmp(name, "late") == 0;
    if (!late && SDL_strcmp(name, "1.03") != 0) {
        return false;
    }
    nw_rev = late;
    return true;
}

const char *nw_rev_name(void)
{
    return nw_rev ? "late" : "1.03";
}
