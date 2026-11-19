#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "platform/paths.h"
#include "vfs/canon.h"
#include "vfs/rnc.h"
#include "vfs/sha256.h"
#include "vfs/vfs.h"

#include "rnc2_fixture.inc"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: CHECK(%s) failed; last error: %s\n", __FILE__, __LINE__, #cond, SDL_GetError()); \
            failures++; \
        } \
    } while (0)

static void hex_of(const void *data, size_t size, char hex[NW_SHA256_HEX])
{
    uint8_t digest[NW_SHA256_SIZE];
    nw_sha256(data, size, digest);
    nw_sha256_hex(digest, hex);
}

static const char old_text[] = "AAAABBBBCCCCDDDDEEEEFFFF";
static const char new_text[] = "AAAABBBBxyzCCCCDDDDFF";
static const char add_text[] = "NU20 stub";

static const uint32_t segs[] = {
    0, 8, 3,
    8, 8, 0,
    20, 2, 0,
    0, 0, 9,
    1200, 400, 2,
    0, 0, 0,
};
static const uint32_t words[] = { 0x007a7978u, 0x3032554eu, 0x75747320u, 0x00000062u, 0x00002121u };

static char old_hex[NW_SHA256_HEX], new_hex[NW_SHA256_HEX], add_hex[NW_SHA256_HEX], rnc_hex[NW_SHA256_HEX];
static char packed_hex[NW_SHA256_HEX];
static uint8_t payload[NW_RNC2_UNPACKED + 2];

static const NwManifest disc = { NULL, 0, "test disc", "SLES_503.86", "1.03", "PAL", NW_REGION_PAL, 0 };
static const NwManifest other = { NULL, 0, "other disc", "SLUS_202.38", "1.00", "NTSC", NW_REGION_US, 0 };

static NwCanonFile files[4];
static NwCanonTable table = { files, 4, segs, sizeof(segs) / sizeof(segs[0]) / 3, words,
                              sizeof(words) / sizeof(words[0]) };

static void build(void)
{
    static const char text[] = "CrashWOC NuWorld RNC2 fixture. ";
    for (unsigned i = 0; i < 1200; i++) {
        payload[i] = (uint8_t)text[i % (sizeof(text) - 1)];
    }
    for (unsigned i = 0; i < NW_RNC2_UNPACKED - 1200; i++) {
        payload[1200 + i] = (uint8_t)(i * 7 + (i >> 3));
    }
    payload[NW_RNC2_UNPACKED] = '!';
    payload[NW_RNC2_UNPACKED + 1] = '!';
    hex_of(old_text, strlen(old_text), old_hex);
    hex_of(new_text, strlen(new_text), new_hex);
    hex_of(add_text, strlen(add_text), add_hex);
    hex_of(nw_rnc2_fixture, sizeof(nw_rnc2_fixture), packed_hex);
    hex_of(payload + 1200, 402, rnc_hex);
    files[0] = (NwCanonFile){ &disc, "LEVELS/A/TEST/TEST.CRT", old_hex, new_hex, (uint32_t)strlen(new_text), 0, 3, 0,
                              false };
    files[1] = (NwCanonFile){ &disc, "LEVELS/A/TEST/FIRE.GSC", NULL, add_hex, (uint32_t)strlen(add_text), 3, 1, 1,
                              false };
    files[2] = (NwCanonFile){ &disc, "LEVELS/A/TEST/TEST.GSC", packed_hex, rnc_hex, 402, 4, 1, 4, true };
    files[3] = (NwCanonFile){ &other, "LEVELS/A/TEST/TEST.CRT", old_hex, new_hex, (uint32_t)strlen(new_text), 0, 3,
                              0, false };
}

static void test_apply(void)
{
    size_t size = 0;
    uint8_t *out;
    CHECK(nw_canon_find(&table, &disc, "LEVELS/A/TEST/TEST.CRT") == &files[0]);
    CHECK(nw_canon_find(&table, &other, "LEVELS/A/TEST/TEST.CRT") == &files[3]);
    CHECK(!nw_canon_find(&table, &disc, "LEVELS/A/TEST/TEST.AI") && !nw_canon_find(NULL, &disc, "X"));

    out = nw_canon_apply(&table, &files[0], (const uint8_t *)old_text, strlen(old_text), &size);
    CHECK(out && size == strlen(new_text) && memcmp(out, new_text, size) == 0);
    SDL_free(out);
    CHECK(!nw_canon_apply(&table, &files[0], (const uint8_t *)new_text, strlen(new_text), &size));
    CHECK(SDL_strstr(SDL_GetError(), "not the file"));

    out = nw_canon_apply(&table, &files[1], NULL, 0, &size);
    CHECK(out && size == strlen(add_text) && memcmp(out, add_text, size) == 0);
    SDL_free(out);

    out = nw_canon_apply(&table, &files[2], nw_rnc2_fixture, sizeof(nw_rnc2_fixture), &size);
    CHECK(out && size == 402 && memcmp(out, payload + 1200, 402) == 0);
    SDL_free(out);

    NwCanonFile bad = files[0];
    bad.result = add_hex;
    CHECK(!nw_canon_apply(&table, &bad, (const uint8_t *)old_text, strlen(old_text), &size));
    CHECK(SDL_strstr(SDL_GetError(), "do not apply"));
    bad = files[0];
    bad.segs = 99;
    CHECK(!nw_canon_apply(&table, &bad, (const uint8_t *)old_text, strlen(old_text), &size));
    bad = files[2];
    bad.seg = 2;
    bad.segs = 1;
    CHECK(!nw_canon_apply(&table, &bad, nw_rnc2_fixture, sizeof(nw_rnc2_fixture), &size));
    NwCanonTable short_words = table;
    short_words.nwords = 3;
    CHECK(!nw_canon_apply(&short_words, &files[2], nw_rnc2_fixture, sizeof(nw_rnc2_fixture), &size));
}

static bool reads(const NwVfs *vfs, const char *path, const void *want, size_t size)
{
    char buf[64];
    NwVfsFile *f = nw_vfs_open(vfs, path, NW_VFS_READ);
    int64_t n = f ? nw_vfs_read(f, buf, sizeof(buf)) : -1;
    bool ok = f && nw_vfs_size(f) == (int64_t)size && n == (int64_t)size && memcmp(buf, want, size) == 0;
    nw_vfs_close(f);
    return ok;
}

static void test_hook(const char *scratch)
{
    char dir[1024], path[1024];
#define AT(name) (SDL_snprintf(path, sizeof(path), "%s/%s", scratch, name), path)
    CHECK(SDL_CreateDirectory(AT("disc/LEVELS/A/TEST")) && SDL_CreateDirectory(AT("mod/LEVELS/A/TEST")));
    CHECK(SDL_SaveFile(AT("disc/LEVELS/A/TEST/TEST.CRT"), old_text, strlen(old_text)));
    CHECK(SDL_SaveFile(AT("disc/LEVELS/A/TEST/TEST.AI"), "plain", 5));
    CHECK(SDL_SaveFile(AT("mod/LEVELS/A/TEST/TEST.CRT"), "mod", 3));
    NwVfs vfs;
    SDL_zero(vfs);
    SDL_snprintf(dir, sizeof(dir), "%s/disc", scratch);
    CHECK(nw_vfs_mount(&vfs, NW_VFS_DISC, dir));
    vfs.manifest = &disc;

    CHECK(reads(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\TEST.CRT;1", old_text, strlen(old_text)));
    CHECK(!nw_vfs_open(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\FIRE.GSC;1", NW_VFS_READ));

    vfs.canon = &table;
    CHECK(reads(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\TEST.CRT;1", new_text, strlen(new_text)));
    CHECK(reads(&vfs, "cdrom0:\\levels\\a\\test\\fire.gsc;1", add_text, strlen(add_text)));
    CHECK(reads(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\TEST.AI;1", "plain", 5));
    NwVfsFile *f = nw_vfs_open(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\TEST.CRT;1", NW_VFS_READ);
    char buf[8];
    CHECK(f && nw_vfs_seek(f, 8, 0) == 8 && nw_vfs_read(f, buf, 3) == 3 && memcmp(buf, "xyz", 3) == 0);
    CHECK(f && nw_vfs_seek(f, -2, 2) == 19 && nw_vfs_read(f, buf, sizeof(buf)) == 2 && memcmp(buf, "FF", 2) == 0);
    CHECK(f && nw_vfs_write(f, "x", 1) == -1);
    nw_vfs_close(f);

    CHECK(SDL_SaveFile(AT("disc/LEVELS/A/TEST/FIRE.GSC"), "own", 3));
    CHECK(nw_vfs_mount(&vfs, NW_VFS_DISC, dir));
    CHECK(reads(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\FIRE.GSC;1", "own", 3));

    SDL_snprintf(dir, sizeof(dir), "%s/mod", scratch);
    CHECK(nw_vfs_mount(&vfs, NW_VFS_OVERLAY, dir));
    CHECK(reads(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\TEST.CRT;1", "mod", 3));
    CHECK(nw_vfs_mount(&vfs, NW_VFS_OVERLAY, NULL));

    CHECK(SDL_SaveFile(AT("disc/LEVELS/A/TEST/TEST.CRT"), "changed", 7));
    CHECK(reads(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\TEST.CRT;1", "changed", 7));

    vfs.manifest = &other;
    CHECK(SDL_SaveFile(AT("disc/LEVELS/A/TEST/TEST.CRT"), old_text, strlen(old_text)));
    CHECK(reads(&vfs, "cdrom0:\\LEVELS\\A\\TEST\\TEST.CRT;1", new_text, strlen(new_text)));
    nw_vfs_shutdown(&vfs);
#undef AT
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: test_canon <scratch dir>\n");
        return 2;
    }
    if (!nw_paths_remove_tree(argv[1]) || !SDL_CreateDirectory(argv[1])) {
        fprintf(stderr, "%s: %s\n", argv[1], SDL_GetError());
        return 1;
    }
    build();
    test_apply();
    test_hook(argv[1]);
    CHECK(nw_canon_table.count > 0 && nw_canon_table.nsegs > 0 && nw_canon_table.nwords > 0);
    for (size_t i = 0; i < nw_canon_table.count; i++) {
        const NwCanonFile *c = &nw_canon_table.files[i];
        CHECK(c->seg <= nw_canon_table.nsegs && c->segs <= nw_canon_table.nsegs - c->seg);
        CHECK(c->disc == &nw_disc_manifest || c->disc == &nw_disc_manifest_usa100 ||
              c->disc == &nw_disc_manifest_pal201);
    }
    nw_paths_remove_tree(argv[1]);
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
