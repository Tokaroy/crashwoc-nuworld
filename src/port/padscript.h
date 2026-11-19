/* Text: `<reads> <buttons> [<lx> <ly> [<rx> <ry>]]` per line, looping; buttons `-` or names joined by `|`. */
#ifndef NUWORLD_PORT_PADSCRIPT_H
#define NUWORLD_PORT_PADSCRIPT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "vfs/vfs.h"

struct nupadframe_s;

int nw_pad_script_hz(const char *spec);
/* Before the game starts; `level` is the run's last level, -1 for none. False with SDL_GetError(). */
bool nw_pad_script_start(const NwVfs *vfs, const char *spec, int level, int hz);
/* In place of the run's script; false with SDL_GetError(). */
bool nw_pad_script_parse(const char *text);
bool nw_pad_script_recording(const uint8_t *data, size_t size);
void nw_pad_script_stop(void);
/* The pad shim's read of port 0: true when the script gave this read's frame. */
bool nw_pad_script_read(struct nupadframe_s *frame);

#endif
