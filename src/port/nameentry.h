/* Any thread unless said. While an entry is open the keyboard types into it instead of being the pad. */
#ifndef NUWORLD_PORT_NAMEENTRY_H
#define NUWORLD_PORT_NAMEENTRY_H

#include <stdbool.h>
#include <stdint.h>

#define NW_NAMEENTRY_BACK '\b'
#define NW_NAMEENTRY_DONE '\r' /* the name as it is: retail's DONE */
#define NW_NAMEENTRY_QUIT 0x1B /* back to the title: retail's TRIANGLE (NEW GAME's only) */

struct cursor_s;
struct nupad_s;

bool nw_nameentry_wanted(void);
/* Main thread: a character (A to Z, 0 to 9, space; others are dropped) or a key above. */
void nw_nameentry_type(int c);
/* Main thread: whether the keyboard is the device used last, which hides the letter table. */
void nw_nameentry_set_keyboard(bool keyboard);
bool nw_nameentry_keyboard(void);
void nw_nameentry_set_pad(bool pad);

#define NW_NAMEENTRY_HINT_LINES 5
#define NW_NAMEENTRY_HINT_TEXT 48

int nw_nameentry_hint(char lines[NW_NAMEENTRY_HINT_LINES][NW_NAMEENTRY_HINT_TEXT], bool pad);

/* Game thread, before retail's ProcMenu; returns the buttons it pressed (DONE and QUIT as retail's CROSS, TRIANGLE). */
uint32_t nw_nameentry_proc(struct cursor_s *cursor, struct nupad_s *pad, uint32_t *taken);
/* Game thread, after retail's ProcMenu. */
void nw_nameentry_done(struct nupad_s *pad, uint32_t pressed, uint32_t taken);
/* Game thread: the longer name being typed ('_' for the letters still to come), NULL with no entry open. */
const char *nw_nameentry_name(void);
/* Preserve the typed name across the autosave question. */
void nw_nameentry_finish(void);
void nw_nameentry_resume_game(const char *name, int position);
/* Game thread, DrawMenu; false when retail draws the menu. */
bool nw_nameentry_draw(struct cursor_s *cursor);

#endif
