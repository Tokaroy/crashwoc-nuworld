#ifndef NUWORLD_GAMEPLAY_GROUND_H
#define NUWORLD_GAMEPLAY_GROUND_H

#include "game/gametypes.h"

struct creature_s;
struct nupad_s;

void nw_ground_apply(void);
/* LoadCharacterModels after*/
void nw_ground_loaded(void);
/* MoveLoopXZ, before retail: turns the slide's heading toward the stick. */
void nw_ground_steer(const struct creature_s *plr, u16 *hdg);
/* MoveLoopXZ, before retail; forward is his speed along the stick. */
void nw_ground_loop(const struct creature_s *plr, float forward);
/* MoveCRASH, around retail. */
void nw_ground_before(struct creature_s *plr, const struct nupad_s *pad);
void nw_ground_after(struct creature_s *plr);
/* AnimateCRASH and AnimateCOCO, after retail. */
void nw_ground_animate(struct creature_s *plr);

#endif
