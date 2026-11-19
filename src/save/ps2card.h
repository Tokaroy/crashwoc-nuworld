/* PCSX2 .ps2 images: 528-byte pages with their ECC, or 512 without; never formatted here (PCSX2 or the PS2 does). */
#ifndef NUWORLD_SAVE_PS2CARD_H
#define NUWORLD_SAVE_PS2CARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NW_PS2CARD_MAGIC "Sony PS2 Memory Card Format "

bool nw_ps2card_is_card(const void *image, size_t size);
/* SDL_malloc'd; NULL with an SDL error, *missing set when the card reads fine but has no such file. */
void *nw_ps2card_file(const void *image, size_t size, const char *dir, const char *name, size_t *len, bool *missing);

typedef struct NwPs2CardFile {
    const char *name;
    const void *data;
    size_t len;
    bool keep; /* a file of the name already there stays as it is */
} NwPs2CardFile;

/* Writes in place; on failure the image may be part-written: keep the file it came from until this succeeds. */
bool nw_ps2card_put(void *image, size_t size, const char *dir, const NwPs2CardFile *files, int count, int64_t time);

#endif
