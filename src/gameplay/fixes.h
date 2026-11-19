#ifndef NUWORLD_GAMEPLAY_FIXES_H
#define NUWORLD_GAMEPLAY_FIXES_H

struct creature_s;
struct obj_s;

void nw_fix_ground_slam(struct creature_s *plr);
float nw_fix_finish_top(struct obj_s *obj);

#endif
