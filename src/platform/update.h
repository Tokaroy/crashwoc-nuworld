#ifndef NUWORLD_PLATFORM_UPDATE_H
#define NUWORLD_PLATFORM_UPDATE_H

#include <stdbool.h>
#include <stddef.h>

#include <SDL3/SDL.h>

#define NW_UPDATE_REPO "denzi-gh/crashwoc-nuworld"
#define NW_UPDATE_STAGE ".update"
#define NW_UPDATE_JOURNAL "replaced.txt"
#define NW_UPDATE_OLD ".old"
#ifdef _WIN32
#define NW_UPDATE_UPDATER "NuWorldUpdater.exe"
#else
#define NW_UPDATE_UPDATER "NuWorldUpdater"
#endif

/* Main thread. The first on starts the check against version, the build's x.y.z; off hides what it found. */
void nw_update_enable(bool on, const char *version);
/* Any thread. */
bool nw_update_available(void);
/* Any thread: the release's page opens in the browser on the next nw_update_poll. */
void nw_update_open(void);
/* Main thread: opens a requested page and minimizes a fullscreen window for it. */
bool nw_update_poll(SDL_Window *window);

void nw_update_args(int argc, char **argv);
bool nw_update_installs(void);
void nw_update_install(void);
void nw_update_cleanup(const char *base);

bool nw_update_tag(const char *json, char *out, size_t cap);
/* Each 1 to 3 dotted numbers after an optional v; *order <0, 0 or >0 as a is older, the same or newer. */
bool nw_update_compare(const char *a, const char *b, int *order);

#endif
