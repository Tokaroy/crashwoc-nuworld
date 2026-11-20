#ifndef NUWORLD_GFX_INTERP_H
#define NUWORLD_GFX_INTERP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gfx/packet.h"

/* Game thread. */
typedef struct NwInterpScope {
    uint64_t obj;
    uint32_t at, n;
    uint16_t joint;
} NwInterpScope;

/* Outside a scope this begins an object, keyed by owner and place, else by tag, src and obj; several of one key
   are told apart by where they were, not by draw order. pose is its matrix, radius its size (0 unknown). */
NwInterpScope nw_interp_begin(const void *src, const void *obj, const void *pose, float radius);
void nw_interp_end(NwInterpScope was);
void nw_interp_at(uint32_t a, uint32_t b, uint32_t c);
/* The object's n joints in model space; parents are signed bytes stride apart, -1 a root. */
void nw_interp_joints(const void *joints, const void *parents, size_t stride, uint32_t n);
void nw_interp_joint(uint32_t joint);
/* Key 0 outside an object and for ops not drawn under the local matrix. */
void nw_interp_cmd(NwCmd *c);
/* The frame is published as p: its snap and its keys, and the frame starts again. */
void nw_interp_publish(NwPacket *p);
/* The decomp's NW_INTERP_KEY, NW_INTERP_SNAP and NW_INTERP_SLOT (nu_port.h). */
void nw_interp_key(const void *key);
void nw_interp_snap(void);
/* Keys top-level draws by owner and place instead of tag, matrix and object; owner NULL ends it. */
void nw_interp_slot(const void *owner, unsigned long long place);
uint32_t nw_interp_frame(void);

/* An object is not the frame before's when it lies more than NW_INTERP_JUMP away, or further than both twice its
   radius (NW_INTERP_STEP when unknown) and four times its last step, or turned over 90 degrees at once. */
#define NW_INTERP_JUMP 16.0f
#define NW_INTERP_STEP 0.5f

/* Render thread. */
typedef struct NwInterpStats {
    uint32_t paired, unpaired;
    uint32_t views;
    uint32_t snapped;          /* b's views that jumped from a's, drawn as b */
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
bool nw_interp_jump(const float a[16], const float b[16]);

#endif
