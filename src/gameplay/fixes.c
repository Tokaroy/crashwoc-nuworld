#include "gameplay/fixes.h"

#include "game/creature.h"
#include "gameplay/crash.h"
#include "gameplay/gameplay.h"

void nw_fix_ground_slam(struct creature_s *plr)
{
    if (nw_profile.bug_fixes && plr->obj.ground != 0 && plr->ok_slam != 0 && plr->jump_frame >= plr->jump_frames) {
        NW_DEVIATION(bug_fixes);
        plr->ok_slam = 0;
    }
}

float nw_fix_finish_top(struct obj_s *obj)
{
    float top = obj->top;
    struct creature_s *c = player;
    if (nw_profile.bug_fixes && obj == &c->obj && (nw_crash_on_foot(c) || nw_coco_on_foot(c)) && top < obj->max.y) {
        NW_DEVIATION(bug_fixes);
        obj->top = obj->max.y;
    }
    return top;
}
