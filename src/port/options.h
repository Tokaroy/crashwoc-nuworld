/* Any thread. The game reads music every frame and SFX when a sound starts (voices too). */
#ifndef NUWORLD_PORT_OPTIONS_H
#define NUWORLD_PORT_OPTIONS_H

#include <stdbool.h>

/* 0-100 */
int nw_options_volume(bool music);
void nw_options_set_volume(bool music, int volume);

#endif
