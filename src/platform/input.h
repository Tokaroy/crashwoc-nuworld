/* Main thread only. */
#ifndef NUWORLD_PLATFORM_INPUT_H
#define NUWORLD_PLATFORM_INPUT_H

#include <SDL3/SDL.h>

#include "platform/controls.h"
#include "shim/shim.h"

/* Opens the first gamepad that connects; when it goes, the next one still connected. */
void nw_input_event(const SDL_Event *event);
void nw_input_poll(bool keyboard);
void nw_input_shutdown(void);

typedef enum NwInputListen {
    NW_INPUT_LISTEN_OFF,
    NW_INPUT_LISTEN_PAD,
    NW_INPUT_LISTEN_KEYS,
} NwInputListen;

void nw_input_set_controls(const NwControls *c);
/* The next press is heard instead of played, the game's pad idles meanwhile; Escape cancels. */
void nw_input_listen(NwInputListen what);
/* -1 while listening or not, else (once) what was heard: a source, 0 for Escape. */
int nw_input_heard(void);
NwPadInput nw_input_last(void);
const char *nw_input_pad_name(void);
const char *nw_input_pad_label(uint16_t source);
NwPromptStyle nw_input_prompt_style(void);

void nw_input_gamepad(SDL_Gamepad *gamepad, const NwControls *c, NwPadInput *in);
/* A stick's keys replace the axis they push. */
void nw_input_keyboard(const bool *keys, const NwControls *c, NwPadInput *in);

#endif
