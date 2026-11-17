/* Wraps of game/cut for the title menu (gameplay/titlemenu.c). */
#include "nu_types.h"
#include "gamelib/gcutscn.h"
#include "game/cut.retail.h"
#include "gameplay/titlemenu.h"
#include "port/level.h"

void StartCutMovie(void)
{
    bool menu = nw_title_menu_starting();
    StartCutMovie__retail();
    if (menu) {
        nw_title_menu_started();
    }
}

void GetSpaceCut(s32 last, s32 next)
{
    if (!nw_cutscene_route(next)) {
        GetSpaceCut__retail(last, next);
    }
}

void SetCutMovieRate(void)
{
    SetCutMovieRate__retail();
    nw_title_menu_rate();
}

void AppCutSceneCharacterRender(struct instNUGCUTSCENE_s *icutscene, struct NUGCUTSCENE_s *cutscene,
                                struct instNUGCUTCHAR_s *icutchar, struct NUGCUTCHAR_s *cutchar, f32 current_frame)
{
    if (!nw_title_menu_render(icutscene, cutscene, icutchar, cutchar, current_frame)) {
        AppCutSceneCharacterRender__retail(icutscene, cutscene, icutchar, cutchar, current_frame);
    }
}
