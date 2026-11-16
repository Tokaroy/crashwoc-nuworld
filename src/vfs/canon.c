#include "vfs/canon.h"

#include <SDL3/SDL.h>

#include "vfs/rnc.h"
#include "vfs/sha256.h"

#include "vfs/canon_table.inc"

const NwCanonTable nw_canon_table = { nw_canon_files, SDL_arraysize(nw_canon_files), nw_canon_segs,
                                      (SDL_arraysize(nw_canon_segs) - 1) / 3, nw_canon_words,
                                      SDL_arraysize(nw_canon_words) - 1 };

const NwCanonFile *nw_canon_find(const NwCanonTable *table, const NwManifest *disc, const char *key)
{
    for (size_t i = 0; table && i < table->count; i++) {
        const NwCanonFile *f = &table->files[i];
        if (f->disc == disc && SDL_strcmp(f->path, key) == 0) {
            return f;
        }
    }
    return NULL;
}

static bool nw_canon_sha(const void *data, size_t size, const char *hex)
{
    uint8_t digest[NW_SHA256_SIZE];
    char got[NW_SHA256_HEX];
    nw_sha256(data, size, digest);
    nw_sha256_hex(digest, got);
    return SDL_strcmp(got, hex) == 0;
}

static bool nw_canon_edit(const NwCanonTable *table, const NwCanonFile *file, const uint8_t *src, size_t len,
                          uint8_t *out)
{
    size_t at = 0, word = file->word;
    if (file->seg > table->nsegs || file->segs > table->nsegs - file->seg) {
        return false;
    }
    for (uint32_t i = 0; i < file->segs; i++) {
        const uint32_t *seg = table->segs + 3 * ((size_t)file->seg + i);
        uint32_t from = seg[0], count = seg[1], bytes = seg[2];
        size_t words = ((size_t)bytes + 3) / 4;
        if (from > len || count > len - from || count > file->size - at || bytes > file->size - at - count ||
            word > table->nwords || words > table->nwords - word) {
            return false;
        }
        if (count) {
            SDL_memcpy(out + at, src + from, count);
            at += count;
        }
        for (uint32_t b = 0; b < bytes; b++) {
            out[at++] = (uint8_t)(table->words[word + b / 4] >> 8 * (b % 4));
        }
        word += words;
    }
    return at == file->size;
}

uint8_t *nw_canon_apply(const NwCanonTable *table, const NwCanonFile *file, const uint8_t *raw, size_t n,
                        size_t *size)
{
    const uint8_t *src = NULL;
    uint8_t *unpacked = NULL;
    size_t len = 0;
    uint32_t full, packed;
    if (file->sha256) {
        if (!raw || !nw_canon_sha(raw, n, file->sha256)) {
            SDL_SetError("%s: not the file the canon table patches", file->path);
            return NULL;
        }
        src = raw;
        len = n;
        if (nw_rnc_header(raw, n, &full, &packed)) {
            unpacked = packed <= n - NW_RNC_HEADER ? SDL_malloc(full ? full : 1) : NULL;
            if (!unpacked || nw_rnc_unpack(raw + NW_RNC_HEADER, raw + NW_RNC_HEADER + packed, unpacked,
                                           unpacked + full) != (int64_t)full) {
                SDL_free(unpacked);
                SDL_SetError("%s: damaged RNC stream", file->path);
                return NULL;
            }
            src = unpacked;
            len = full;
        }
    }
    uint8_t *out = SDL_malloc(file->size ? file->size : 1);
    bool ok = out && nw_canon_edit(table, file, src, len, out) && nw_canon_sha(out, file->size, file->result);
    SDL_free(unpacked);
    if (!ok) {
        SDL_free(out);
        SDL_SetError("%s: the canon edits do not apply", file->path);
        return NULL;
    }
    *size = file->size;
    return out;
}

uint8_t *nw_canon_load(const NwCanonTable *table, const NwCanonFile *file, const char *path, size_t *size)
{
    size_t n = 0;
    uint8_t *raw = path ? SDL_LoadFile(path, &n) : NULL;
    if (path && !raw) {
        return NULL;
    }
    uint8_t *out = nw_canon_apply(table, file, raw, n, size);
    SDL_free(raw);
    return out;
}
