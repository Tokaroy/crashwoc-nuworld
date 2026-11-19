#ifndef NUWORLD_GAMEPLAY_AIRINDICATOR_H
#define NUWORLD_GAMEPLAY_AIRINDICATOR_H

#include <stdbool.h>

struct creature_s;

/* alpha is 0-1; false for no ring. Reads only. */
bool nw_air_indicator_at(const struct creature_s *c, float *y, float *alpha);
/* DrawCreatures' main pass for Character[0], after retail drew him. */
void nw_air_indicator_draw(struct creature_s *c);
/* NuMtlRelease (a level load) freed the ring's material. */
void nw_air_indicator_release(void);

#endif
