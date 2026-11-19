/* --trace-sound vblanks count from the logos cut's first hold (or --level's start), as tools/pcsx2_trace.py's. */
#ifndef NUWORLD_PORT_TRACE_H
#define NUWORLD_PORT_TRACE_H

#include <stdbool.h>

void nw_trace_sound_start(void);
/* The running EE thread, after a vblank's interrupts ran (stdout). */
void nw_trace_sound_vblank(void);
bool nw_trace_vblank(int vbcnt, int *vblank);
/* The label tools/pcsx2_capture.py gives a PCSX2 savestate (stdout). */
void nw_trace_frame(int vblank);

#endif
