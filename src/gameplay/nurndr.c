#include "nu3d/nurndr.h"
#include "nu3d/nurndr.retail.h"
#include "gameplay/fixes.h"

void NuHGobjEval(struct NUHGOBJ_s *hobj, int njanims, struct NUJOINTANIM_s *janim, struct numtx_s *mtx_array)
{
    NuHGobjEval__retail(hobj, njanims, janim, mtx_array);
    nw_fix_goggles_place(hobj, mtx_array);
}
