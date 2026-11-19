/* --dump/--poke: PS2-layout game state for tools/lockstep.py; vblanks count from the title cut's start. */
#ifndef NUWORLD_PORT_DUMP_H
#define NUWORLD_PORT_DUMP_H

#include <stdbool.h>
#include <stddef.h>

/* list: a comma list or a file of "frame N" / "vblank N" lines; superbuffer bytes only with heap. */
bool nw_dump_start(const char *list, const char *dir, bool heap);
/* "symbol=value": dump vblanks count from the first vblank on which it holds; pokes still count from the title cut. */
bool nw_dump_after(const char *cond);
/* "vblank:symbol=value" 32-bit writes; a symbol is a data image name, D_<address> or 0x<address>, optional +offset. */
bool nw_poke_start(const char *list);

/* Game thread, at the yield point before vblank vbcnt's handlers, where PCSX2 saves its state. */
void nw_dump_vblank(int vbcnt);

bool nw_dump_finished(void);
unsigned nw_dump_written(unsigned *failed);

bool nw_dump_write(const char *path, int vblank, int vbcnt, bool heap, char *err, size_t err_size);

#endif
