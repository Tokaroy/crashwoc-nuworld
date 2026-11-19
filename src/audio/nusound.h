/* NUSOUND.IRX runs on the calling thread; its stream thread's ticks catch up at each call. */
#ifndef NUWORLD_AUDIO_NUSOUND_H
#define NUWORLD_AUDIO_NUSOUND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NW_NUSOUND_RPC 0x12345
#define NW_NUSOUND_REPLY 0x200

struct NwVfs;

/* now_us is the module's clock (NULL: no time passes). */
void nw_nusound_init(const struct NwVfs *vfs, uint64_t (*now_us)(void));

/* ProcessEECommand: function 0 runs the batch in send; every call copies the reply into recv. */
void nw_nusound_rpc(unsigned fno, const void *send, size_t ssize, void *recv, size_t rsize);

void nw_nusound_cd_moved(void);

/* On (the default) the drive and the SPU DMA take their PCSX2 time, off none. Any thread; init keeps it. */
void nw_nusound_set_hardware_speed(bool on);

/* Streams started from now on start `seconds` in, silent `quiet` seconds, then fade in over `rise` (game thread). */
void nw_nusound_skip(float seconds, float quiet, float rise);

#endif
