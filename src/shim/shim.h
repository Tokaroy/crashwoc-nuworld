/* Unimplemented sce* and EE kernel calls report to the stub telemetry as unit "sdk". */
#ifndef NUWORLD_SHIM_SHIM_H
#define NUWORLD_SHIM_SHIM_H

#include <stdint.h>

#include "port/stub.h"

#define NW_SHIM_STUB(name, address) NW_STUB("sdk", name, address, "not in the sdk shim yet")

struct NwVfs;

void nw_newlib_init(void);

/* NULL makes every open fail; closes what is open. */
void nw_shim_file_init(const struct NwVfs *vfs);
const struct NwVfs *nw_shim_file_vfs(void);

/* Buttons are active-high libpad bits; sticks 0x00-0xFF centred on 0x80 (y down); l2/r2 the trigger pressure. */
#define NW_PAD_L2 0x0001u
#define NW_PAD_R2 0x0002u
#define NW_PAD_L1 0x0004u
#define NW_PAD_R1 0x0008u
#define NW_PAD_TRIANGLE 0x0010u
#define NW_PAD_CIRCLE 0x0020u
#define NW_PAD_CROSS 0x0040u
#define NW_PAD_SQUARE 0x0080u
#define NW_PAD_SELECT 0x0100u
#define NW_PAD_L3 0x0200u
#define NW_PAD_R3 0x0400u
#define NW_PAD_START 0x0800u
#define NW_PAD_UP 0x1000u
#define NW_PAD_RIGHT 0x2000u
#define NW_PAD_DOWN 0x4000u
#define NW_PAD_LEFT 0x8000u

typedef struct NwPadInput {
    uint16_t buttons;
    uint8_t lx, ly, rx, ry;
    uint8_t l2, r2;
} NwPadInput;

/* Any thread; the pad stays idle until the first call. */
void nw_pad_set(const NwPadInput *in);
/* Any thread: the motors the game set last, the small one 0 or 1, the big one 0-255. */
void nw_pad_motors(uint8_t *small, uint8_t *big);
struct nupadframe_s;
void nw_pad_frame(const NwPadInput *in, struct nupadframe_s *data);

/* The return from an interrupt: the best ready EE thread runs. */
void nw_ee_schedule(void);
/* A yield point of the threads other than main (file reads): delivers the vblanks due. */
void nw_ee_yield(void);
/* NW_HOST_IDLE (decomp nu_port.h): an EE thread's final spin waits for the next vblank. */
void nw_ee_idle(void);
/* The game parks at its next yield point once `n` threads were terminated (0: never). */
void nw_ee_park_after(unsigned n);
/* The threads terminated so far, and the vblanks of up to `max` of them. */
unsigned nw_ee_terminated(uint64_t *at, unsigned max);

#endif
