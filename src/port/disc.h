#ifndef NUWORLD_PORT_DISC_H
#define NUWORLD_PORT_DISC_H

#include <stdbool.h>

/* The US disc */
bool nw_disc_ntsc(void);
/* A level files sound number in PALs numbering. */
int nw_disc_level_sfx(int sfx);

#endif
