/* Main thread. */
#ifndef NUWORLD_PLATFORM_DISPLAY_H
#define NUWORLD_PLATFORM_DISPLAY_H

#include <stdbool.h>

#include <SDL3/SDL.h>

typedef enum NwWindowMode {
    NW_WINDOW_WINDOWED,
    NW_WINDOW_BORDERLESS, /* fullscreen at the desktop's mode */
    NW_WINDOW_FULLSCREEN, /* exclusive: the display takes the window's mode */
    NW_WINDOW_MODES
} NwWindowMode;

typedef enum NwVsync {
    NW_VSYNC_OFF,
    NW_VSYNC_ON,
    NW_VSYNC_ADAPTIVE, /* late frames swap at once */
    NW_VSYNC_MODES
} NwVsync;

typedef struct NwResolution {
    int w, h; /* pixels; 0 x 0 = auto */
} NwResolution;

/* False with an SDL error, the window as it was. */

bool nw_display_set_mode(SDL_Window *window, NwWindowMode mode, NwResolution res);


const SDL_DisplayMode *nw_display_pick(SDL_DisplayMode *const *modes, int count, NwResolution res);


/* Largest first: the fullscreen modes, for windowed those that fit the usable area; none borderless. */
int nw_display_sizes(SDL_Window *window, NwWindowMode mode, NwResolution *sizes, int max);


NwResolution nw_display_fullscreen(SDL_Window *window, NwWindowMode mode, NwResolution res);


/* Windows GL takes a monitor-sized window off the compositor (capture freezes): fullscreen gets one more column. */
void nw_display_compose(SDL_Window *window);


/* Without that column: what the frame fills. */
void nw_display_size(SDL_Window *window, int *w, int *h);


/* Hz, 0 when unknown. */
float nw_display_refresh(SDL_Window *window);


/* Returns the one in effect: adaptive falls back to on. */
NwVsync nw_display_set_vsync(NwVsync vsync);

#endif
