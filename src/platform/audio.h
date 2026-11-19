#ifndef NUWORLD_PLATFORM_AUDIO_H
#define NUWORLD_PLATFORM_AUDIO_H

#include <stdbool.h>

bool nw_audio_open(void);
void nw_audio_close(void);

/* 0-100 over the game's own volumes; voice is NUSOUND channel 4. Main thread; the device need not be open. */
void nw_audio_set_buses(int master, int voice);

#endif
