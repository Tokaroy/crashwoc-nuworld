/* <root>import/ is for the Steam Deck, where Game Mode has neither file dialogs nor drops. */
#ifndef NUWORLD_FRONTEND_WIZARD_H
#define NUWORLD_FRONTEND_WIZARD_H

#include <SDL3/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NwWizardResult {
    NW_WIZARD_OPEN,
    NW_WIZARD_CLOSED,
    NW_WIZARD_QUIT,
} NwWizardResult;

typedef struct NwWizard NwWizard;

/* root's import/ folder must already exist. */
NwWizard *nw_wizard_create(SDL_Window *window, const char *root);

/* Needs the GL context still current. */
void nw_wizard_destroy(NwWizard *w);

void nw_wizard_event(NwWizard *w, const SDL_Event *e);

/* Draws one frame between nw_ui_begin and nw_ui_end. */
NwWizardResult nw_wizard_draw(NwWizard *w);

#ifdef __cplusplus
}
#endif

#endif
