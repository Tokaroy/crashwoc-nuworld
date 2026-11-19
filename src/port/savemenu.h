#ifndef NUWORLD_PORT_SAVEMENU_H
#define NUWORLD_PORT_SAVEMENU_H

#include <stdbool.h>

struct cursor_s;
struct nupad_s;

void nw_savemenu_platform(bool deck);
/* After retail's ProcMenu (game/game wrap), with the cursor's column before it. */
void nw_savemenu_proc(struct cursor_s *cursor, struct nupad_s *pad, int old_x);
/* Before retail's DrawMenu: a save grid opened outside ProcMenu (the title's LOAD GAME) gets its page too. */
void nw_savemenu_prepare(struct cursor_s *cursor);
/* After retail's DrawMenu. */
void nw_savemenu_draw(struct cursor_s *cursor);
/* Game thread. */
bool nw_savemenu_open(void);
bool nw_savemenu_grid_open(void);

#endif
