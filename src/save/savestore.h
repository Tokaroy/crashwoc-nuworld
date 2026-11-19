/* Save N is saveNNN.toml (NNN = N + 1); deletion leaves a tombstone so backups are not resurrected. */
#ifndef NUWORLD_SAVE_SAVESTORE_H
#define NUWORLD_SAVE_SAVESTORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "save/savecodec.h"

#define NW_SAVE_GAMES 100
#define NW_SAVE_AUTOS 100                /* independent autosaves, never manual destinations */
#define NW_SAVE_TOTAL (NW_SAVE_GAMES + NW_SAVE_AUTOS)
#define NW_SAVE_PS2_PAL "BESLES-50386" /* a PS2 save's folder and file name prefix */
#define NW_SAVE_PS2_US "BASLUS-20238"
#define NW_SAVE_PS2_GAMES 4

typedef enum NwSaveResult {
    NW_SAVE_OK,
    NW_SAVE_NONE,
    NW_SAVE_FAILED,
    NW_SAVE_BACKUP, /* OK, from the .bak: the SDL error says why the file itself did not load */
} NwSaveResult;

/* Local time; all zero when unknown. */
typedef struct NwSaveTime {
    int year, month, day, hour, minute, second;
} NwSaveTime;

/* Absent in old/imported saves; campaign identifies a playthrough independently of its name. */
typedef struct NwSaveMeta {
    char campaign[33];
    bool autosave;
    int64_t revision;
} NwSaveMeta;

/* n >= NW_SAVE_GAMES addresses autosaveNNN.toml; generation: 0 current, 1 .bak, 2 .bak2 (no fall forward). */
NwSaveResult nw_save_load_meta(const char *dir, int n, int generation, const NwSaveRegistry *reg, uint8_t *game,
                              NwSaveNames *names, NwSaveTime *saved, NwSaveMeta *meta);
bool nw_save_store_meta(const char *dir, int n, const NwSaveRegistry *reg, const uint8_t *game,
                        const NwSaveNames *names, const NwSaveTime *saved, const NwSaveMeta *meta);
int nw_save_scan_auto(const char *dir, int *last);

/* dir ends with a separator; outputs are set only on an OK result; a newer format fails and is kept. */
NwSaveResult nw_save_load(const char *dir, int n, const NwSaveRegistry *reg, uint8_t *game, NwSaveNames *names,
                          NwSaveTime *saved);
/* names NULL: none; saved NULL: now. */
bool nw_save_store(const char *dir, int n, const NwSaveRegistry *reg, const uint8_t *game, const NwSaveNames *names,
                   const NwSaveTime *saved);
/* Moves save n's file to .bak; NW_SAVE_NONE when it has none. */
NwSaveResult nw_save_remove(const char *dir, int n);
bool nw_save_exists(const char *dir, int n);
/* Whether dir has had saves: a save's file, or the .bak a deleted one leaves. */
bool nw_save_started(const char *dir);
/* The save written last by file time (ties: the higher), -1 without; *last (may be NULL): the highest save. */
int nw_save_scan(const char *dir, int *last);

/* Before dir's first save, once; the older file is renamed .converted. The saves made, -1 with an SDL error. */
int nw_save_convert(const char *dir, const NwSaveRegistry *reg);
/* The saves made, 0 when file holds no save, -1 with an SDL error (also with fewer free saves than games). */
int nw_save_import(const char *dir, const char *file, const NwSaveRegistry *reg);
/* Into the first free saves, all or none. */
int nw_save_add(const char *dir, const NwSaveRegistry *reg, const uint8_t *games, int count, int *numbers);
/* Each game's manual save with the same record, -1 for none. */
int nw_save_match(const char *dir, const NwSaveRegistry *reg, const uint8_t *games, int count, int *numbers);

#define NW_SAVE_PS2_KEEP -1  /* the card's game stays (an empty slot when it has none) */
#define NW_SAVE_PS2_EMPTY -2 /* emptied, as the game deletes one */

typedef enum NwSaveCard {
    NW_SAVE_CARD_FAILED, /* an SDL error: no such file, not a card, a save there that does not read */
    NW_SAVE_CARD_BLANK,  /* a card no PS2 has formatted yet (the SDL error says to format it in PCSX2) */
    NW_SAVE_CARD_NONE,   /* formatted, without the game's save */
    NW_SAVE_CARD_SAVE,   /* the game's save: its games are read */
} NwSaveCard;

/* card is a PCSX2 .ps2 image; games gets NW_SAVE_PS2_GAMES records. */
NwSaveCard nw_save_card(const char *card, const char *ps2_name, const NwSaveRegistry *reg, uint8_t *games);
/* picks: per PS2 slot a save's number or NW_SAVE_PS2_KEEP / _EMPTY; on failure the card is unchanged. */
bool nw_save_export(const char *dir, const NwSaveRegistry *reg, const char *card, const char *ps2_name,
                    const int *picks, const void *icon, size_t icon_len);

/* time is an SDL_Time; all zero when it does not convert. */
void nw_save_time_from(int64_t time, NwSaveTime *t);

#endif
