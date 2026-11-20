#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "gfx/interp.h"

static int failures;

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++; \
        } \
    } while (0)

static bool near(const float *a, const float *b, int n, float eps)
{
    for (int i = 0; i < n; i++) {
        if (fabsf(a[i] - b[i]) > eps) {
            return false;
        }
    }
    return true;
}

static void yaw(float m[16], float angle, float s, float x, float y, float z)
{
    float c = cosf(angle), n = sinf(angle);
    const float r[16] = { c * s, 0, -n * s, 0, 0, s, 0, 0, n * s, 0, c * s, 0, x, y, z, 1 };
    memcpy(m, r, sizeof(r));
}

static void mul(float out[16], const float a[16], const float b[16])
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            out[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] +
                             a[i * 4 + 3] * b[12 + j];
        }
    }
}

static void drop(NwPacket *p)
{
    SDL_free(p->cmds);
    SDL_free(p->views);
    SDL_free(p->joints);
    SDL_free(p->weights);
    SDL_free(p->bodies);
    SDL_free(p->bones);
    SDL_free(p->parents);
    SDL_free(p->ptllists);
    SDL_free(p->ptls);
    memset(p, 0, sizeof(*p));
}

static uint64_t seq;

static void publish(NwPacket *p)
{
    nw_interp_publish(p);
    p->seq = ++seq;
    p->edge = seq;
}

static NwCmd *draw(NwPacket *p, NwCmdOp op, const float m[16])
{
    NwCmd *c = nw_packet_cmd(p);
    c->op = (uint8_t)op;
    memcpy(c->local, m, sizeof(c->local));
    nw_interp_cmd(c);
    return c;
}

/* Returns the cmd's index. */
static uint32_t object(NwPacket *p, const void *src, const void *obj, const float m[16], float radius)
{
    NwInterpScope s = nw_interp_begin(src, obj, m, radius);
    draw(p, NW_CMD_DRAW, m);
    nw_interp_end(s);
    return p->ncmds - 1;
}

static void test_mtx(void)
{
    float a[16], b[16], out[16], want[16];
    yaw(a, 0.3f, 2.0f, 1, 2, 3);
    yaw(b, 1.2f, 1.0f, 5, 6, 7);
    nw_interp_mtx(out, a, b, 0.0f);
    CHECK(near(out, a, 16, 1e-5f));
    nw_interp_mtx(out, a, b, 1.0f);
    CHECK(near(out, b, 16, 1e-5f));
    /* A 170-degree turn goes through 85 degrees, not through a shrunk matrix. */
    yaw(a, 0.0f, 1.0f, 0, 0, 0);
    yaw(b, 170.0f * (float)M_PI / 180.0f, 1.0f, 10, 0, 0);
    nw_interp_mtx(out, a, b, 0.5f);
    yaw(want, 85.0f * (float)M_PI / 180.0f, 1.0f, 5, 0, 0);
    CHECK(near(out, want, 16, 1e-4f));
    /* The short way round: 350 degrees is -10. */
    yaw(b, 350.0f * (float)M_PI / 180.0f, 1.0f, 0, 0, 0);
    nw_interp_mtx(out, a, b, 0.5f);
    yaw(want, -5.0f * (float)M_PI / 180.0f, 1.0f, 0, 0, 0);
    CHECK(near(out, want, 16, 1e-4f));
    /* A mirror (a reflection's y flip) blends as one. */
    yaw(a, 0.2f, 1.0f, 0, 0, 0);
    yaw(b, 0.6f, 1.0f, 0, 4, 0);
    a[5] = b[5] = -1.0f;
    nw_interp_mtx(out, a, b, 0.5f);
    yaw(want, 0.4f, 1.0f, 0, 2, 0);
    want[5] = -1.0f;
    CHECK(near(out, want, 16, 1e-4f));
    memset(a, 0, sizeof(a));
    memset(b, 0, sizeof(b));
    a[11] = 1.0f;
    b[11] = 3.0f;
    nw_interp_mtx(out, a, b, 0.5f);
    CHECK(out[11] == 2.0f && out[0] == 0.0f);
}

static void frame(NwPacket *p, int skip_first, float x)
{
    static float inst[2][16];
    static float stack[16];
    static int gobj;
    float m[16];
    NwCmd *clear = nw_packet_cmd(p);
    clear->op = NW_CMD_CLEAR;
    clear->clear.mask = 3;
    for (int i = 0; i < 2; i++) {
        yaw(m, 0.0f, 1.0f, x + (float)i, 0, 0);
        NwInterpScope s = nw_interp_begin(inst[i], &gobj, m, 1.0f);
        nw_interp_at(0, 0, 0);
        if (!(skip_first && i == 0)) {
            draw(p, NW_CMD_DRAW, m);
        }
        nw_interp_end(s);
    }
    for (int i = 0; i < 2; i++) {
        if (skip_first && i == 0) {
            continue;
        }
        yaw(m, 0.0f, 1.0f, x + 10.0f + (float)i, 0, 0);
        nw_interp_key(i ? &inst[1] : &inst[0]);
        object(p, stack, &gobj, m, 1.0f);
        nw_interp_key(NULL);
    }
    NwCmd *rect = nw_packet_cmd(p);
    rect->op = NW_CMD_RECT;
    nw_interp_cmd(rect);
    publish(p);
}

static void test_keys(void)
{
    NwPacket a = { 0 }, b = { 0 };
    frame(&a, 0, 0.0f);
    frame(&b, 1, 10.0f);
    a.edge = 10;
    b.edge = 12;
    CHECK(a.ncmds == 6 && b.ncmds == 4);
    CHECK(a.cmds[1].key && a.cmds[1].key != a.cmds[2].key && a.cmds[3].key != a.cmds[4].key);
    CHECK(a.cmds[5].op == NW_CMD_RECT && a.cmds[5].key == 0);
    CHECK(b.cmds[1].key == a.cmds[2].key && b.cmds[2].key == a.cmds[4].key);
    CHECK(nw_interp_redraws(&b));

    CHECK(nw_interp_t(&a, &b, 12.0) == 0.0f && nw_interp_t(&a, &b, 13.0) == 0.5f && nw_interp_t(&a, &b, 20.0) == 1.0f);
    CHECK(nw_interp_t(NULL, &b, 12.0) == 1.0f);
    const NwPacket *out = nw_interp_blend(&a, &b, 0.5f);
    CHECK(out != &b && out->ncmds == b.ncmds);
    CHECK(fabsf(out->cmds[1].local[12] - 6.0f) < 1e-4f && fabsf(out->cmds[2].local[12] - 16.0f) < 1e-4f);
    CHECK(b.cmds[1].local[12] == 11.0f);
    NwInterpStats st = nw_interp_stats();
    CHECK(st.paired == 2 && st.unpaired == 0);
    CHECK(nw_interp_blend(&a, &b, 1.0f) == &b);

    NwCmd first = b.cmds[0];
    b.cmds[0] = b.cmds[1];
    b.cmds[1] = first;
    CHECK(!nw_interp_redraws(&b));
    drop(&a);
    drop(&b);
}

static void test_slot(void)
{
    NwPacket p[2] = { 0 };
    static int owner, glyph[2], mtx[2];
    float m[16];
    uint32_t frame = nw_interp_frame();
    yaw(m, 0.0f, 1.0f, 0, 0, 0);
    for (int k = 0; k < 2; k++) {
        nw_interp_slot(&owner, 7);
        object(&p[k], &mtx[k], &glyph[k], m, 1.0f);
        nw_interp_slot(NULL, 0);
        object(&p[k], &mtx[0], &glyph[k], m, 1.0f);
        publish(&p[k]);
    }
    CHECK(nw_interp_frame() == frame + 2);
    CHECK(p[0].cmds[0].key && p[0].cmds[0].key == p[1].cmds[0].key);
    CHECK(p[0].cmds[1].key != p[1].cmds[1].key && p[0].cmds[1].key != p[0].cmds[0].key);
    drop(&p[0]);
    drop(&p[1]);
}

static void test_snaps(void)
{
    NwPacket a = { 0 }, b = { 0 };
    frame(&a, 0, 0.0f);
    nw_interp_snap();
    frame(&b, 0, 1.0f);
    a.edge = 20;
    b.edge = 21;
    CHECK(b.snap && !a.snap && nw_interp_t(&a, &b, 21.0) == 1.0f);
    b.snap = false;
    CHECK(nw_interp_t(&a, &b, 21.0) == 0.0f);
    b.edge = 20 + NW_INTERP_GAP + 1;
    CHECK(nw_interp_t(&a, &b, (double)b.edge) == 1.0f);

    float m[16], turned[16];
    yaw(m, 0.0f, 1.0f, 0, 0, 0);
    yaw(turned, 1.7f, 1.0f, 0, 0, 0);
    CHECK(!nw_interp_jump(m, m) && nw_interp_jump(m, turned));
    yaw(turned, 0.5f, 1.0f, NW_INTERP_JUMP + 1.0f, 0, 0);
    CHECK(nw_interp_jump(m, turned));
    drop(&a);
    drop(&b);
}

static void test_order(void)
{
    static int stack, gobj;
    NwPacket p[4] = { 0 };
    float m[16];
    yaw(m, 0.0f, 1.0f, 0, 0, 0);
    uint32_t x0 = object(&p[0], &stack, &gobj, m, 0.2f);
    yaw(m, 0.0f, 1.0f, 5, 0, 0);
    uint32_t y0 = object(&p[0], &stack, &gobj, m, 0.2f);
    publish(&p[0]);
    yaw(m, 0.0f, 1.0f, 5.1f, 0, 0);
    uint32_t y1 = object(&p[1], &stack, &gobj, m, 0.2f);
    publish(&p[1]);
    CHECK(p[1].cmds[y1].key == p[0].cmds[y0].key && p[1].cmds[y1].key != p[0].cmds[x0].key);
    yaw(m, 0.0f, 1.0f, 0, 0, 0);
    uint32_t z2 = object(&p[2], &stack, &gobj, m, 0.2f);
    yaw(m, 0.0f, 1.0f, 5.2f, 0, 0);
    uint32_t y2 = object(&p[2], &stack, &gobj, m, 0.2f);
    publish(&p[2]);
    CHECK(p[2].cmds[y2].key == p[1].cmds[y1].key);
    CHECK(p[2].cmds[z2].key != p[0].cmds[x0].key && p[2].cmds[z2].key != p[2].cmds[y2].key);
    yaw(m, 0.0f, 1.0f, 5.3f, 0, 0);
    uint32_t y3 = object(&p[3], &stack, &gobj, m, 0.2f);
    yaw(m, 0.0f, 1.0f, 0, 0, 0);
    uint32_t z3 = object(&p[3], &stack, &gobj, m, 0.2f);
    publish(&p[3]);
    CHECK(p[3].cmds[y3].key == p[2].cmds[y2].key && p[3].cmds[z3].key == p[2].cmds[z2].key);
    const NwPacket *out = nw_interp_blend(&p[2], &p[3], 0.5f);
    CHECK(fabsf(out->cmds[y3].local[12] - 5.25f) < 1e-4f && out->cmds[z3].local[12] == 0.0f);
    for (int k = 0; k < 4; k++) {
        drop(&p[k]);
    }
}

/* Shots two steps apart: each is as near to where the one ahead was as to where it was itself. */
static void test_stream(void)
{
    static int stack, gobj;
    NwPacket p[6] = { 0 };
    uint64_t keys[6][8] = { { 0 } };
    float m[16];
    for (int k = 0; k < 6; k++) {
        for (int s = 0; s * 2 <= k; s++) {
            yaw(m, 0.0f, 1.0f, 1.2f * (float)(k - s * 2), 0, 0);
            object(&p[k], &stack, &gobj, m, 0.05f);
        }
        publish(&p[k]);
        for (int s = 0; s * 2 <= k; s++) {
            keys[k][s] = p[k].cmds[s].key;
        }
    }
    for (int k = 1; k < 6; k++) {
        for (int s = 0; s * 2 <= k - 1; s++) {
            CHECK(keys[k][s] == keys[k - 1][s]);
        }
        for (int s = 0; s * 2 <= k; s++) {
            for (int o = 0; o < s; o++) {
                CHECK(keys[k][s] != keys[k][o]);
            }
        }
    }
    for (int k = 0; k < 6; k++) {
        drop(&p[k]);
    }
}

static void test_jumps(void)
{
    static int inst, gobj, spinner, flipper;
    NwPacket p[6] = { 0 };
    const float x[6] = { 0.0f, 0.1f, 0.2f, 3.0f, 3.1f, 3.2f };
    uint32_t at[6][3];
    float m[16];
    for (int k = 0; k < 6; k++) {
        yaw(m, 0.0f, 1.0f, x[k], 0, 0);
        at[k][0] = object(&p[k], &inst, &gobj, m, 0.25f);
        yaw(m, 2.1f * (float)k, 1.0f, 0, 5, 0);
        at[k][1] = object(&p[k], &spinner, &gobj, m, 0.25f);
        yaw(m, k >= 4 ? 3.0f : 0.0f, 1.0f, 0, 9, 0);
        at[k][2] = object(&p[k], &flipper, &gobj, m, 0.25f);
        publish(&p[k]);
    }
#define KEY(k, i) p[k].cmds[at[k][i]].key
    CHECK(KEY(1, 0) == KEY(0, 0) && KEY(2, 0) == KEY(1, 0));
    CHECK(KEY(3, 0) != KEY(2, 0));
    CHECK(KEY(4, 0) == KEY(3, 0) && KEY(5, 0) == KEY(4, 0));
    /* A spinner's first turn reads as a cut. */
    CHECK(KEY(1, 1) != KEY(0, 1));
    CHECK(KEY(2, 1) == KEY(1, 1) && KEY(3, 1) == KEY(2, 1) && KEY(5, 1) == KEY(4, 1));
    CHECK(KEY(3, 2) == KEY(0, 2) && KEY(4, 2) != KEY(3, 2) && KEY(5, 2) == KEY(4, 2));
#undef KEY
    nw_interp_blend(&p[2], &p[3], 0.5f);
    NwInterpStats st = nw_interp_stats();
    CHECK(st.paired == 2 && st.unpaired == 1);
    for (int k = 0; k < 6; k++) {
        drop(&p[k]);
    }
}

static void test_mirror(void)
{
    static int stack, gobj;
    NwPacket p[2] = { 0 };
    float m[16], r[16];
    uint32_t real[2], image[2];
    for (int k = 0; k < 2; k++) {
        yaw(m, 0.1f * (float)k, 1.0f, 0, 0, 0);
        memcpy(r, m, sizeof(r));
        r[5] = -1.0f;
        if (k == 0) {
            real[k] = object(&p[k], &stack, &gobj, m, 0.5f);
        }
        image[k] = object(&p[k], &stack, &gobj, r, 0.5f);
        if (k == 1) {
            real[k] = object(&p[k], &stack, &gobj, m, 0.5f);
        }
        publish(&p[k]);
    }
    CHECK(p[1].cmds[real[1]].key == p[0].cmds[real[0]].key);
    CHECK(p[1].cmds[image[1]].key == p[0].cmds[image[0]].key);
    CHECK(p[0].cmds[real[0]].key != p[0].cmds[image[0]].key);
    drop(&p[0]);
    drop(&p[1]);
}

static void view(NwPacket *p, const float cam[16])
{
    uint32_t index;
    NwView *v = nw_packet_view(p, &index);
    memset(v, 0, sizeof(*v));
    memcpy(v->cam, cam, sizeof(v->cam));
    memcpy(v->vs, cam, sizeof(v->vs));
    v->camscale[0] = v->camscale[1] = v->camscale[2] = 1.0f;
}

static void test_views(void)
{
    NwPacket p[3] = { 0 };
    float world[16], panel[16], far[16];
    yaw(panel, 0.0f, 1.0f, 0, 0, 0);
    yaw(world, 0.4f, 1.0f, 3, 2, 1);
    view(&p[0], world);
    view(&p[0], panel);
    publish(&p[0]);
    yaw(world, 0.45f, 1.0f, 3.1f, 2, 1);
    view(&p[1], world);
    view(&p[1], world);
    view(&p[1], panel);
    view(&p[1], world);
    publish(&p[1]);
    CHECK(p[0].views[0].key && p[0].views[1].key && p[0].views[0].key != p[0].views[1].key);
    CHECK(p[1].views[0].key == p[0].views[0].key && p[1].views[1].key == p[0].views[0].key);
    CHECK(p[1].views[2].key == p[0].views[1].key && p[1].views[3].key == p[0].views[0].key);
    nw_interp_blend(&p[0], &p[1], 0.5f);
    CHECK(nw_interp_stats().views == 4);
    yaw(far, 0.45f, 1.0f, 300, 2, 1);
    view(&p[2], far);
    view(&p[2], panel);
    publish(&p[2]);
    CHECK(p[2].views[0].key != p[1].views[0].key && p[2].views[1].key == p[1].views[2].key);
    for (int k = 0; k < 3; k++) {
        drop(&p[k]);
    }
}

static void test_epoch(void)
{
    static int inst, gobj;
    NwPacket a = { 0 }, b = { 0 };
    float m[16];
    yaw(m, 0.0f, 1.0f, 0, 0, 0);
    NwInterpScope s = nw_interp_begin(&inst, &gobj, m, 1.0f);
    NwCmd late = { .op = NW_CMD_DRAW };
    nw_interp_cmd(&late);
    nw_interp_end(s);
    CHECK(late.key != 0);
    publish(&a);
    object(&b, &inst, &gobj, m, 1.0f);
    *nw_packet_cmd(&b) = late;
    publish(&b);
    CHECK(b.cmds[0].key != 0 && b.cmds[1].key == 0);
    drop(&a);
    drop(&b);
}

#define BONES 3

/* Three joints in a row, 2 apart along x; angle[j] turns joint j. A skin draw and a part on the last joint. */
static void rig(NwPacket *p, const float wm[16], const float angle[BONES], const void *hobj)
{
    static const signed char parents[BONES] = { -1, 0, 1 };
    float bones[BONES][16], local[16], part[16];
    for (int j = 0; j < BONES; j++) {
        yaw(local, angle[j], 1.0f, j ? 2.0f : 0.0f, 0, 0);
        if (j) {
            mul(bones[j], local, bones[j - 1]);
        } else {
            memcpy(bones[j], local, sizeof(local));
        }
    }
    NwCmd *clear = nw_packet_cmd(p);
    clear->op = NW_CMD_CLEAR;
    clear->clear.mask = 1;
    NwInterpScope s = nw_interp_begin(wm, hobj, wm, 4.0f);
    nw_interp_joints(bones, parents, 1, BONES);
    nw_interp_at(0, 0, 2);
    nw_interp_joint(2);
    mul(part, bones[2], wm);
    draw(p, NW_CMD_DRAW, part);
    nw_interp_joint(NW_JOINT_NONE);
    nw_interp_at(0, 2, 0);
    NwCmd *skin = draw(p, NW_CMD_SKIN, wm);
    float(*pal)[16] = nw_packet_joints(p, BONES, &skin->first);
    skin->count = BONES;
    for (int j = 0; j < BONES; j++) {
        yaw(local, 0.0f, 1.0f, -2.0f * (float)j, 0, 0);
        mul(pal[j], local, bones[j]);
    }
    float *w = nw_packet_weights(p, 1, &skin->weights);
    skin->nweights = 1;
    w[0] = angle[0];
    nw_interp_end(s);
    publish(p);
}

/* The end of joint j's bone, skinned. */
static void tip(float out[3], const float pal[16], int j)
{
    float x = 2.0f * (float)j + 2.0f;
    for (int k = 0; k < 3; k++) {
        out[k] = x * pal[k] + pal[12 + k];
    }
}

static void test_skin(void)
{
    static int hobj;
    NwPacket p[4] = { 0 };
    float wm[16], want[16];
    /* Bones keep their length and stay joined; matrices blended one by one would cut the corner. */
    const float rest[BONES] = { 0.0f, 0.0f, 0.0f }, swung[BONES] = { 0.0f, 1.5f, 0.0f };
    yaw(wm, 0.0f, 1.0f, 10, 0, 0);
    rig(&p[0], wm, rest, &hobj);
    yaw(wm, 0.0f, 1.0f, 11, 0, 0);
    rig(&p[1], wm, swung, &hobj);
    const NwPacket *out = nw_interp_blend(&p[0], &p[1], 0.5f);
    const NwCmd *part = &out->cmds[1], *skin = &out->cmds[2];
    CHECK(part->op == NW_CMD_DRAW && skin->op == NW_CMD_SKIN && part->body && part->joint == 2);
    yaw(want, 0.0f, 1.0f, 10.5f, 0, 0);
    CHECK(near(skin->local, want, 16, 1e-4f));
    float elbow[3], hand[3], end[3];
    tip(elbow, out->joints[skin->first], 0);
    tip(hand, out->joints[skin->first + 1], 1);
    tip(end, out->joints[skin->first + 2], 2);
    CHECK(fabsf(elbow[0] - 2.0f) < 1e-4f && fabsf(elbow[2]) < 1e-4f);
    float dx = hand[0] - elbow[0], dz = hand[2] - elbow[2];
    CHECK(fabsf(sqrtf(dx * dx + dz * dz) - 2.0f) < 1e-4f);
    CHECK(fabsf(dx - 2.0f * cosf(0.75f)) < 1e-4f && fabsf(dz + 2.0f * sinf(0.75f)) < 1e-4f);
    dx = end[0] - hand[0];
    dz = end[2] - hand[2];
    CHECK(fabsf(sqrtf(dx * dx + dz * dz) - 2.0f) < 1e-4f);
    CHECK(fabsf(part->local[12] - (hand[0] + 10.5f)) < 1e-4f && fabsf(part->local[14] - hand[2]) < 1e-4f);
    CHECK(out->weights[skin->weights] == 0.0f && p[1].weights[0] == 0.0f);
    CHECK(p[1].cmds[1].local[12] != part->local[12]);

    const float cut[BONES] = { 0.4f, 1.5f + 3.0f, 0.0f };
    rig(&p[2], wm, cut, &hobj);
    out = nw_interp_blend(&p[1], &p[2], 0.5f);
    skin = &out->cmds[2];
    tip(elbow, out->joints[skin->first], 0);
    tip(hand, out->joints[skin->first + 1], 1);
    CHECK(fabsf(elbow[0] - 2.0f * cosf(0.2f)) < 1e-4f && fabsf(elbow[2] + 2.0f * sinf(0.2f)) < 1e-4f);
    dx = hand[0] - elbow[0];
    dz = hand[2] - elbow[2];
    CHECK(fabsf(dx - 2.0f * cosf(0.2f + 4.5f)) < 1e-4f && fabsf(dz + 2.0f * sinf(0.2f + 4.5f)) < 1e-4f);
    CHECK(fabsf(out->weights[skin->weights] - 0.2f) < 1e-6f);

    rig(&p[3], wm, cut, &hobj);
    out = nw_interp_blend(&p[2], &p[3], 0.5f);
    CHECK(memcmp(out->cmds[1].local, p[3].cmds[1].local, sizeof(p[3].cmds[1].local)) == 0);
    CHECK(memcmp(out->joints, p[3].joints, BONES * sizeof(*p[3].joints)) == 0);
    for (int k = 0; k < 4; k++) {
        drop(&p[k]);
    }
}

static void test_particles(void)
{
    static int chunk, list;
    NwPacket p[3] = { 0 };
    const float time[3] = { 1.0f, 2.0f, 0.5f };
    float m[16];
    for (int k = 0; k < 3; k++) {
        yaw(m, 0.0f, 1.0f, (float)k, 0, 0);
        NwCmd *clear = nw_packet_cmd(&p[k]);
        clear->op = NW_CMD_CLEAR;
        clear->clear.mask = 1;
        NwInterpScope s = nw_interp_begin(&chunk, &list, m, 0.0f);
        NwCmd *c = draw(&p[k], NW_CMD_PARTICLES, m);
        NwPtlList *l = nw_packet_ptllist(&p[k], &c->ptllist);
        memset(l, 0, sizeof(*l));
        l->head[3] = time[k];
        nw_interp_end(s);
        publish(&p[k]);
    }
    const NwPacket *out = nw_interp_blend(&p[0], &p[1], 0.5f);
    CHECK(out->ptllists[0].head[3] == 1.5f && out->cmds[1].local[12] == 0.5f);
    out = nw_interp_blend(&p[1], &p[2], 0.5f);
    CHECK(out->ptllists[0].head[3] == 0.5f && out->cmds[1].local[12] == 2.0f);
    for (int k = 0; k < 3; k++) {
        drop(&p[k]);
    }
}

static void test_uv(void)
{
    NwPacket p[2] = { 0 };
    static int mtl[2], tex[2];
    const float u[2] = { 0.95f, 0.05f }, v[2] = { 0.2f, 0.4f };
    for (int k = 0; k < 2; k++) {
        uint32_t index;
        for (int m = 0; m < 2; m++) {
            NwMtlSnap *s = nw_packet_mtl(&p[k], &index);
            memset(s, 0, sizeof(*s));
            s->key = &mtl[m];
            s->wrap = 1;
            s->tex = (const NwGpuTex *)&tex[m ? k : 0];
            s->vu[0] = u[k];
            s->vu[1] = v[k];
            s->row[0] = (int32_t)(u[k] * 4096.0f);
            s->row[1] = (int32_t)(v[k] * 4096.0f);
        }
        p[k].seq = ++seq;
        p[k].edge = seq;
    }
    const NwPacket *out = nw_interp_blend(&p[0], &p[1], 0.25f);
    CHECK(fabsf(out->mtls[0].vu[0] - 0.975f) < 1e-5f && fabsf(out->mtls[0].vu[1] - 0.25f) < 1e-5f);
    CHECK(out->mtls[0].row[0] == (int32_t)(out->mtls[0].vu[0] * 4096.0f));
    out = nw_interp_blend(&p[0], &p[1], 0.75f);
    CHECK(fabsf(out->mtls[0].vu[0] - 0.025f) < 1e-5f);
    CHECK(out->mtls[1].vu[0] == 0.05f && out->mtls[1].row[1] == p[1].mtls[1].row[1]);
    for (int k = 0; k < 2; k++) {
        SDL_free(p[k].mtls);
    }
}

int main(void)
{
    test_mtx();
    test_keys();
    test_slot();
    test_snaps();
    test_order();
    test_stream();
    test_jumps();
    test_mirror();
    test_views();
    test_epoch();
    test_skin();
    test_particles();
    test_uv();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("interp: ok\n");
    return 0;
}
