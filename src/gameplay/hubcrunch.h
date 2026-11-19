#ifndef NUWORLD_GAMEPLAY_HUBCRUNCH_H
#define NUWORLD_GAMEPLAY_HUBCRUNCH_H

struct creature_s;
struct nupad_s;

/* MovePlayer, before retail; takes the X that starts the minigame. */
struct nupad_s *nw_crunch_pad(struct creature_s *c, struct nupad_s *pad);
/* MovePlayer, after retail. */
void nw_crunch_push(struct creature_s *c);
/* DrawCreatures' main pass for Character[0]. */
void nw_crunch_draw(void);
/* DrawPanel's end. */
void nw_crunch_prompt(void);
/* CloseCreatures: the level's textures and materials go next. */
void nw_crunch_close(void);

#endif
