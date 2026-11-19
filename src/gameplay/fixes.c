#include "gameplay/fixes.h"

#include "game/creature.h"
#include "gameplay/gameplay.h"

void nw_fix_ground_slam(struct creature_s *plr)
{
    if (nw_profile.bug_fixes && plr->obj.ground != 0 && plr->ok_slam != 0 && plr->jump_frame >= plr->jump_frames) {
        NW_DEVIATION(bug_fixes);
        plr->ok_slam = 0;
    }
}
