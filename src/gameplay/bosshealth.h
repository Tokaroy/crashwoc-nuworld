#ifndef NUWORLD_GAMEPLAY_BOSSHEALTH_H
#define NUWORLD_GAMEPLAY_BOSSHEALTH_H

#include <stdbool.h>

/* 0 for a bar without phases (Air Crunch's 100). */
int nw_boss_health_lines(int maxhealth);
/* percent as the glider's health is shown: "  5%", " 73%", "100%". */
void nw_boss_health_percent(int percent, char text[8]);
/* Crunch Time's health (0-4) for its attack phase, as the GameCube's GetCurrentSpaceBossObjectives. */
int nw_boss_health_crunch_time_hp(unsigned phase);
/* DrawPanel, before its debris. */
void nw_boss_health_crunch_time(void);
/* DrawPanel's Crunch head; false leaves it to retail. */
bool nw_boss_health_head(unsigned short xrot, unsigned short yrot, int rot, float x, float y, float z, float scale);
/* BossBar's x, before retail draws it. */
float nw_boss_health_bar_x(float x);
/* BossBar, after retail drew it. */
void nw_boss_health_bar(float x, float y, float z, float xs, float ys, int health, int maxhealth);
/* NuMtlRelease (a level load) freed the lines' material. */
void nw_boss_health_release(void);

#endif
