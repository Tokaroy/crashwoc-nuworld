#ifndef NUWORLD_GAMEPLAY_MONKEYBARS_H
#define NUWORLD_GAMEPLAY_MONKEYBARS_H

struct creature_s;

/* At boot after the data image and the 60 Hz table, then between frames. */
void nw_monkey_bars_apply(void);
/* AnimateCRASH, after retail picked the action and before MovePlayer advances it. */
void nw_monkey_bars_animate(struct creature_s *plr);

#endif
