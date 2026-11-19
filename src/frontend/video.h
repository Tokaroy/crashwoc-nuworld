#ifndef NUWORLD_FRONTEND_VIDEO_H
#define NUWORLD_FRONTEND_VIDEO_H

#include "frontend/settings.h"
#include "platform/platform.h"

/* Returns the picture's aspect for nw_wide_set. */
float nw_video_start(NwPlatform *p, NwSettings *s);
void nw_video_event(const SDL_Event *e);
/* s's cap, else the display's refresh when VSync is off; 0 none (VSync paces). */
unsigned nw_video_fps(const NwSettings *s);
void nw_video_apply(const NwSettings *s, const NwSettings *was);

#endif
