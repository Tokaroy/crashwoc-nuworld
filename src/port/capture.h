#ifndef NUWORLD_PORT_CAPTURE_H
#define NUWORLD_PORT_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

/* PCSX2's savestate shows a frame a few vblanks old, so frames from BEFORE before to AFTER after are saved. */
#define NW_CAPTURE_BEFORE 3
#define NW_CAPTURE_AFTER 1

bool nw_capture_start(const char *list);
/* Game thread, at each vblank. */
void nw_capture_vblank(void);
/* Game thread: waits while the renderer saves packet seq, when it is wanted. */
void nw_capture_published(uint64_t seq, int vbcnt);
/* Render thread: the packet the game thread waits on, with its trace vblank; 0 when none. */
uint64_t nw_capture_wanted(int *vblank);
/* Render thread. */
void nw_capture_release(void);
bool nw_capture_finished(void);

#endif
