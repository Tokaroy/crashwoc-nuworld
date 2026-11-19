#ifndef NUWORLD_GFX_INTERP_H
#define NUWORLD_GFX_INTERP_H

#include <stdbool.h>
#include <stdint.h>

#include "gfx/packet.h"

/* Game thread. */
typedef struct NwInterpScope {
    uint64_t obj;
    uint32_t at, n;
} NwInterpScope;

/* Draws until nw_interp_end key by their place inside an enclosing object, else by tag, src, obj and ordinal. */
NwInterpScope nw_interp_begin(const void *src, const void *obj);
void nw_interp_end(NwInterpScope was);
void nw_interp_at(uint32_t a, uint32_t b, uint32_t c);
/* 0 outside an object and for ops not drawn under the local matrix. */
uint64_t nw_interp_cmd(NwCmdOp op);
/* NuRndrSetViewMtx's key: which call of the frame it is. */
uint32_t nw_interp_view(void);
/* The frame is published as p: its snap, and the counts start again. */
void nw_interp_publish(NwPacket *p);
/* The decomp's NW_INTERP_KEY, NW_INTERP_SNAP and NW_INTERP_SLOT (nu_port.h). */
void nw_interp_key(const void *key);
void nw_interp_snap(void);
/* Keys top-level draws by owner and place instead of tag, matrix and object; owner NULL ends it. */
void nw_interp_slot(const void *owner, unsigned long long place);
uint32_t nw_interp_frame(void);

/* Render thread. */
typedef struct NwInterpStats {
    uint32_t paired, unpaired;
    uint32_t views;
    uint32_t snapped;          /* b's draws and views that jumped from a's, drawn as b */
} NwInterpStats;

/* 0 a, 1 b; always 1 when b is a cut or comes more than NW_INTERP_GAP edges after a (a load, a stall). */
#define NW_INTERP_GAP 4
float nw_interp_t(const NwPacket *a, const NwPacket *b, double now);
/* p draws over nothing it left before: it clears the back buffer before it draws there, so it can run again. */
bool nw_interp_redraws(const NwPacket *p);
/* b, or a copy of it blended from a by t, which lives until the next call. */
const NwPacket *nw_interp_blend(const NwPacket *a, const NwPacket *b, float t);
NwInterpStats nw_interp_stats(void);

/* Affine row-vector matrices (rows 0-2 axes, 3 position): rotation slerped, lengths and position lerped. */
void nw_interp_mtx(float out[16], const float a[16], const float b[16], float t);
/* b is not where a went: position more than NW_INTERP_JUMP away or axes turned more than 90 degrees. */
#define NW_INTERP_JUMP 16.0f
bool nw_interp_jump(const float a[16], const float b[16]);

#endif
