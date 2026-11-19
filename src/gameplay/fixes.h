#ifndef NUWORLD_GAMEPLAY_FIXES_H
#define NUWORLD_GAMEPLAY_FIXES_H

struct creature_s;
struct numtx_s;
struct obj_s;
struct NUHGOBJ_s;

void nw_fix_ground_slam(struct creature_s *plr);
float nw_fix_finish_top(struct obj_s *obj);
void nw_fix_goggles_before(struct creature_s *c);
void nw_fix_goggles_after(void);
void nw_fix_goggles_place(struct NUHGOBJ_s *hobj, struct numtx_s *mtx_array);

#endif
