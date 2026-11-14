/* Wraps of gamelib/edbits: the title menu's particles stay quiet, a level file's sounds follow the disc. */
#include "nu_types.h"
#include "numath/numathtypes.h"
#include "gamelib/edbits.retail.h"
#include "gameplay/titlemenu.h"
#include "port/disc.h"

/* Declared by no header. */
extern s32 debris_sfx;

void edbitsSoundPlay(struct nuvec_s *pos, int sfx)
{
    if (debris_sfx && nw_title_menu_quiet()) {
        debris_sfx = 0; /* as retail's call */
        return;
    }
    edbitsSoundPlay__retail(pos, nw_disc_level_sfx(sfx));
}
