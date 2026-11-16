#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import math
import os
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
OUT = REPO / "src" / "vfs" / "canon_table.inc"
SKIP = 77
SOURCES = (("pal103", "pal", "nw_disc_manifest"), ("usa100", "us", "nw_disc_manifest_usa100"),
           ("pal201", "pal201", "nw_disc_manifest_pal201"))
TARGET = "us101"
KINDS = {"CRT", "AI", "OBJ", "WMP", "TER", "PTL", "ANM", "CPT", "VIS", "GSC"}
LEVELS = ("LEVELS/A/", "LEVELS/C/")
COUPLED = {"GSC", "TER", "OBJ", "ANM", "VIS"}
NOISE = 1e-3
WORLD = 0.02
MTL_SIZE = 0x1D0
MTL_LOOSE = set(range(0x120, 0x16C, 4)) | {0x1A8, 0x1C0, 0x1CC}


class Refused(Exception):
    pass


def unpack(raw: bytes) -> bytes:
    if raw[:4] != b"RNC\x02":
        return raw
    if len(raw) < 18:
        raise ValueError("RNC header")
    size, packed = struct.unpack_from(">II", raw, 4)
    d = raw[18:18 + packed]
    out = bytearray()
    pos = buf = n = 0

    def bit():
        nonlocal pos, buf, n
        if n == 0:
            buf = d[pos]
            pos += 1
            n = 8
        n -= 1
        return (buf >> n) & 1

    bit()
    bit()
    while True:
        if not bit():
            out.append(d[pos])
            pos += 1
            continue
        count, hi, ln = 2, True, 0
        if not bit():
            count = (count << 1) | bit()
            if bit():
                count = ((count - 1) << 1) | bit()
                if count == 9:
                    v = 0
                    for _ in range(4):
                        v = (v << 1) | bit()
                    k = 4 * (v + 3)
                    out += d[pos:pos + k]
                    pos += k
                    continue
        elif not bit():
            hi = False
        else:
            count = 3
            if bit():
                count = d[pos]
                pos += 1
                if count == 0:
                    if bit():
                        continue
                    break
                count += 8
        if hi and bit():
            ln = bit()
            if bit():
                ln = ((ln << 1) | bit()) | 4
                if not bit():
                    ln = (ln << 1) | bit()
            elif ln == 0:
                ln = 2 | bit()
            ln <<= 8
        off = (ln | d[pos]) + 1
        pos += 1
        s = len(out) - off
        if s < 0:
            raise ValueError("RNC back reference")
        if off >= count:
            out += out[s:s + count]
        else:
            for i in range(count):
                out.append(out[s + i])
    if len(out) != size:
        raise ValueError("RNC size")
    return bytes(out)


def match(a: bytes, ao: int, b: bytes, bo: int) -> int:
    m = min(len(a) - ao, len(b) - bo)
    n = 0
    step = 4096
    while step:
        while n + step <= m and a[ao + n:ao + n + step] == b[bo + n:bo + n + step]:
            n += step
        step //= 8
    while n < m and a[ao + n] == b[bo + n]:
        n += 1
    return n


def delta(src: bytes, dst: bytes, k: int = 16) -> list[tuple[int, int, bytes]]:
    step = 4 if len(src) < 1 << 20 else 16
    index: dict[bytes, int] = {}
    for o in range(0, len(src) - k + 1, step):
        index.setdefault(src[o:o + k], o)
    segs: list[list] = [[0, 0, bytearray()]]
    p = 0
    last = None
    while p < len(dst):
        best_o, best_n = -1, 0
        cands = []
        if last is not None:
            cands.append(last[0] + p - last[1])
        o = index.get(dst[p:p + k])
        if o is not None:
            cands.append(o)
        for o in cands:
            if 0 <= o < len(src):
                n = match(src, o, dst, p)
                if n > best_n:
                    best_o, best_n = o, n
        if best_n < k and not (best_n >= 4 and p + best_n == len(dst)):
            segs[-1][2].append(dst[p])
            p += 1
            continue
        lit = segs[-1][2]
        back = 0
        while lit and best_o > 0 and src[best_o - 1] == lit[-1]:
            lit.pop()
            best_o -= 1
            best_n += 1
            back += 1
        segs.append([best_o, best_n, bytearray()])
        p += best_n - back
        last = (best_o + best_n, p)
    if segs[0][1] == 0 and not segs[0][2] and len(segs) > 1:
        segs.pop(0)
    return [(o, n, bytes(lit)) for o, n, lit in segs]


def near(a: bytes, b: bytes, tol: float = NOISE) -> bool:
    if len(a) != len(b):
        return False
    if a == b:
        return True
    n = len(a) // 4 * 4
    if a[n:] != b[n:]:
        return False
    for (x,), (y,) in zip(struct.iter_unpack("<I", a[:n]), struct.iter_unpack("<I", b[:n])):
        if x != y:
            if (x & 0x7FFFFFFF and not x & 0x7F800000) or (y & 0x7FFFFFFF and not y & 0x7F800000):
                return False
            fx = struct.unpack("<f", struct.pack("<I", x))[0]
            fy = struct.unpack("<f", struct.pack("<I", y))[0]
            if not (math.isfinite(fx) and math.isfinite(fy)) or abs(fx - fy) > tol * max(1.0, abs(fx)):
                return False
    return True


class Scene:
    def __init__(self, nu: bytes):
        if nu[:4] != b"NU20" or len(nu) < 16:
            raise Refused("not an NU20 scene")
        self.nu = nu
        self.order = []
        self.chunks = {}
        off = 16
        while off < len(nu):
            size = abs(struct.unpack_from("<i", nu, off + 4)[0])
            tag = nu[off:off + 4].decode("latin1")
            if size < 8 or off + size > len(nu) or tag in self.chunks:
                raise Refused("chunk walk")
            self.order.append(tag)
            self.chunks[tag] = (off, size)
            off += size
        self.objects = self.read_objects() if "OBJ0" in self.chunks else []
        self.instances = self.records("INST", 0x50)
        self.anims = self.records("IABL", 0x60)
        self.names = self.data("NTBL")[4:4 + self.i32("NTBL", 0)] if "NTBL" in self.chunks else b""
        self.specials = [(self.name(struct.unpack_from("<i", r, 0x44)[0]), struct.unpack_from("<i", r, 0x40)[0])
                         for r in self.records("SPEC", 0x50)]
        self.splines = self.read_splines()

    def data(self, tag: str) -> bytes:
        off, size = self.chunks[tag]
        return self.nu[off + 8:off + size]

    def i32(self, tag: str, at: int) -> int:
        return struct.unpack_from("<i", self.data(tag), at)[0]

    def records(self, tag: str, size: int) -> list[bytes]:
        if tag not in self.chunks:
            return []
        d = self.data(tag)
        n = struct.unpack_from("<i", d, 0)[0]
        if n < 0 or 8 + n * size > len(d):
            raise Refused(f"{tag} count")
        return [d[8 + i * size:8 + (i + 1) * size] for i in range(n)]

    def name(self, off: int) -> str:
        if off < 0 or off >= len(self.names):
            raise Refused("name offset")
        end = self.names.find(b"\0", off)
        return self.names[off:end if end >= 0 else len(self.names)].decode("latin1")

    def read_splines(self) -> list[tuple[str, int, bytes]]:
        if "SST0" not in self.chunks:
            return []
        d = self.data("SST0")
        n = struct.unpack_from("<i", d, 0)[0]
        p, out = 8, []
        for _ in range(n):
            ln, _size, name = struct.unpack_from("<HHi", d, p)
            out.append((self.name(name), ln, d[p + 8:p + 8 + 12 * ln]))
            p += 8 + 12 * ln
        return out

    def read_objects(self) -> list[tuple[int, int, tuple, tuple, list[int]]]:
        off, size = self.chunks["OBJ0"]
        nu, p = self.nu, off + 8
        n, colourref = struct.unpack_from("<ii", nu, p)
        p += 8
        out = []
        for _ in range(n):
            start = p
            nsub = struct.unpack_from("<i", nu, p)[0]
            p += 16
            lo, hi, mtls = [math.inf] * 3, [-math.inf] * 3, []
            for _ in range(nsub):
                kind = struct.unpack_from("<i", nu, p)[0]
                count = struct.unpack_from("<i", nu, p + 4)[0]
                if kind == 0:
                    p += 32
                    for _ in range(count):
                        skip, mtl = struct.unpack_from("<ii", nu, p + 4)
                        mtls.append(mtl)
                        p += 12 + skip
                        p += 4 + struct.unpack_from("<i", nu, p)[0]
                elif kind == 1:
                    p += 16
                    for _ in range(count):
                        mtls.append(struct.unpack_from("<i", nu, p)[0])
                        p += 16 + struct.unpack_from("<i", nu, p + 12)[0]
                else:
                    raise Refused("geom type")
                a = struct.unpack_from("<3f", nu, p)
                b = struct.unpack_from("<3f", nu, p + 12)
                lo = [min(x, y) for x, y in zip(lo, a)]
                hi = [max(x, y) for x, y in zip(hi, b)]
                p += 32
            if colourref == 1:
                p += 16 + struct.unpack_from("<i", nu, p)[0]
            out.append((start, p, tuple(lo), tuple(hi), mtls))
        if p != off + size:
            raise Refused("object walk")
        return out

    def obj_bytes(self, i: int) -> bytes:
        return self.nu[self.objects[i][0]:self.objects[i][1]]

    def world(self, r: bytes) -> tuple:
        m = struct.unpack_from("<16f", r)
        objid, flags, anim = struct.unpack_from("<iii", r, 0x40)
        if not 0 <= objid < len(self.objects):
            raise Refused("instance object")
        lo, hi = self.objects[objid][2], self.objects[objid][3]
        pts = [tuple(x * m[c] + y * m[4 + c] + z * m[8 + c] + m[12 + c] for c in range(3))
               for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
        return (tuple(min(q[c] for q in pts) for c in range(3)) + tuple(max(q[c] for q in pts) for c in range(3)),
                flags, anim != 0)

    def anim_of(self, i: int) -> bytes | None:
        if struct.unpack_from("<i", self.instances[i], 0x48)[0] == 0:
            return None
        k = sum(1 for r in self.instances[:i] if struct.unpack_from("<i", r, 0x48)[0] != 0)
        return self.anims[k]

    def material(self, i: int) -> bytes:
        tag = "MS00" if "MS00" in self.chunks else "MS02"
        d = self.data(tag)
        if not 0 <= i < struct.unpack_from("<i", d, 0)[0]:
            raise Refused("material index")
        return d[8 + i * MTL_SIZE:8 + (i + 1) * MTL_SIZE]

    def texture(self, i: int) -> bytes:
        d = self.data("TST0" if "TST0" in self.chunks else "TST2")
        n, p = struct.unpack_from("<i", d, 0)[0], 8
        for k in range(n):
            size, skip = struct.unpack_from("<ii", d, p)
            p += 16 if skip == 0 else 8 + skip
            if k == i:
                return d[p:p + size]
            p += size
        raise Refused("texture index")


def wnear(a: tuple, b: tuple) -> bool:
    return a[1:] == b[1:] and all(abs(x - y) <= WORLD * max(1.0, abs(x)) for x, y in zip(a[0], b[0]))


def align(s: Scene, t: Scene) -> dict[int, int]:
    ws = [s.world(r) for r in s.instances]
    wt = [t.world(r) for r in t.instances]
    m, used = {}, set()
    for i in range(min(len(ws), len(wt))):
        if wnear(ws[i], wt[i]):
            m[i] = i
            used.add(i)
    grid: dict[tuple, list[int]] = {}
    for j, w in enumerate(ws):
        if j not in used:
            grid.setdefault(tuple(round((w[0][c] + w[0][c + 3]) / 2) for c in range(3)), []).append(j)
    for i, w in enumerate(wt):
        if i in m:
            continue
        c = tuple(round((w[0][q] + w[0][q + 3]) / 2) for q in range(3))
        cands = sorted(j for dx in (-1, 0, 1) for dy in (-1, 0, 1) for dz in (-1, 0, 1)
                       for j in grid.get((c[0] + dx, c[1] + dy, c[2] + dz), []))
        for j in cands:
            if j not in used and wnear(ws[j], w):
                m[i] = j
                used.add(j)
                break
    return m


def ter_platforms(ter: bytes | None) -> dict[int, tuple[bytes, bytes]]:
    if not ter:
        return {}
    d = struct.unpack_from("<i", ter, 0)[0]
    n = struct.unpack_from("<i", ter, 2 * d)[0]
    out, at = {}, 4
    for i in range(n):
        e = 2 * d + 4 + i * 52
        off = struct.unpack_from("<i", ter, e)[0]
        kind, info = struct.unpack_from("<hh", ter, e + 16)
        if kind == 1:
            out.setdefault(info, (ter[e + 20:e + 52], ter[at:at + 2 * off]))
        at += 2 * off
    return out


def material_compatible(s: Scene, t: Scene, i: int) -> bool:
    a, b = s.material(i), t.material(i)
    if any(a[o:o + 4] != b[o:o + 4] for o in range(0, MTL_SIZE, 4) if o not in MTL_LOOSE):
        return False
    tid = struct.unpack_from("<i", a, 0x1A4)[0]
    return tid == -1 or s.texture(tid) == t.texture(tid)


def chunk(tag: str, body: bytes) -> bytes:
    return tag.encode("latin1") + struct.pack("<i", 8 + len(body)) + body


def instance_record(s: Scene, t: Scene, item, objects: list[int], adopt: dict) -> bytes:
    if isinstance(item, int):
        rec = s.instances[item]
        i = adopt.get(("old", item))
        if i is None:
            return rec
        ti = t.instances[i]
        objid = len(s.objects) + objects.index(struct.unpack_from("<i", ti, 0x40)[0])
        return ti[:0x40] + struct.pack("<i", objid) + rec[0x44:]
    if item[0] == "new":
        ti = t.instances[item[1]]
        objid = len(s.objects) + objects.index(struct.unpack_from("<i", ti, 0x40)[0])
        return ti[:0x40] + struct.pack("<ii", objid, struct.unpack_from("<i", ti, 0x44)[0]) + b"\0\0\0\0" + ti[0x4C:]
    blank = next((r for r in s.instances if struct.unpack_from("<i", r, 0x48)[0] == 0), None)
    if blank is None:
        raise Refused("no instance to hide")
    return blank[:0x44] + struct.pack("<i", struct.unpack_from("<i", blank, 0x44)[0] & ~1) + blank[0x48:]


def scene_patch(old: bytes, new: bytes, ter_new: bytes | None, ter_old: bytes | None) -> tuple[bytes, list[str]]:
    s, t = Scene(old), Scene(new)
    if "INID" in s.chunks or "INID" in t.chunks or set(s.order) != set(t.order):
        raise Refused("chunk set")
    if "OBJ0" not in s.chunks or "INST" not in s.chunks:
        raise Refused("no objects")
    if s.data("OBJ0")[4:8] != t.data("OBJ0")[4:8]:
        raise Refused("colour references")
    notes = []
    m = align(s, t)
    plats_new, plats_old = ter_platforms(ter_new), ter_platforms(ter_old)
    adopt: dict = {}
    for i in sorted(set(plats_new) | {i for _n, i in t.specials}):
        if not 0 <= i < len(t.instances):
            raise Refused(f"reference {i} past the instances")
        ti = t.instances[i]
        tobj = struct.unpack_from("<i", ti, 0x40)[0]
        j = m.get(i)
        if j is None:
            if struct.unpack_from("<i", ti, 0x48)[0] != 0:
                raise Refused(f"new animated instance {i}")
            adopt[("new", i)] = i
            continue
        sj = s.instances[j]
        pivot = near(sj[:0x40], ti[:0x40])
        if i in plats_new:
            was = plats_old.get(j)
            same = was is not None and was[1] == plats_new[i][1]
            if not pivot and not same:
                raise Refused(f"platform {i} moved with its frame")
            if not same and not near(s.obj_bytes(struct.unpack_from("<i", sj, 0x40)[0]), t.obj_bytes(tobj)):
                adopt[("old", j)] = i
        if not pivot:
            notes.append(f"{i} keeps the old pivot")
    objects = []
    for i in sorted(adopt.values()):
        tobj = struct.unpack_from("<i", t.instances[i], 0x40)[0]
        if not all(material_compatible(s, t, k) for k in t.objects[tobj][4]):
            raise Refused(f"instance {i}: its materials differ")
        if tobj not in objects:
            objects.append(tobj)
    items: list = list(range(len(s.instances)))
    where = {x: k for k, x in enumerate(items)}

    def add(item) -> None:
        where[item] = len(items)
        items.append(item)

    def put(slot: int, item) -> None:
        while len(items) < slot:
            add(("hole", len(items)))
        if item not in where:
            add(item)
        if len(items) == slot:
            add(("hole", slot))
        p, q = where[item], items[slot]
        items[slot], items[p] = item, q
        where[item], where[q] = slot, p

    def item_of(i: int):
        return m[i] if i in m else ("new", i)

    for i in sorted(plats_new):
        put(i, item_of(i))
    for _n, i in t.specials:
        if item_of(i) not in where:
            add(item_of(i))
    holes = sum(1 for x in items if not isinstance(x, int) and x[0] == "hole")
    if holes:
        notes.append(f"{holes} hidden instances")
    slots = [instance_record(s, t, x, objects, adopt) for x in items]
    anims = []
    for x, rec in zip(items, slots):
        if struct.unpack_from("<i", rec, 0x48)[0] != 0:
            if not isinstance(x, int) or s.anim_of(x) is None:
                raise Refused("an animated instance without its animation")
            anims.append(s.anim_of(x))
    names_same = s.names == t.names
    old_spec = [(n, where[j]) for n, j in s.specials]
    new_spec = [(n, where[item_of(i)]) for n, i in t.specials]
    spl_same = len(s.splines) == len(t.splines) and all(
        a[0] == b[0] and a[1] == b[1] and near(a[2], b[2]) for a, b in zip(s.splines, t.splines))
    take = set()
    if not spl_same:
        take |= {"SST0"} if names_same else {"NTBL", "SPEC", "SST0"}
    if old_spec != new_spec:
        take |= {"SPEC"} if names_same else {"NTBL", "SPEC", "SST0"}
    if "NTBL" in take and "TAS0" in s.chunks:
        raise Refused("texture animations name the old table")
    rebuilt = {}
    spec_src = t if "SPEC" in take else s
    spec_map = new_spec if "SPEC" in take else old_spec
    spec_recs = spec_src.records("SPEC", 0x50)
    recs = [r[:0x40] + struct.pack("<i", k) + r[0x44:] for r, (_n, k) in zip(spec_recs, spec_map)]
    if recs != spec_recs or "SPEC" in take:
        rebuilt["SPEC"] = chunk("SPEC", spec_src.data("SPEC")[:8] + b"".join(recs))
    take.discard("SPEC")
    if slots != s.instances:
        rebuilt["INST"] = chunk("INST", struct.pack("<i", len(slots)) + s.data("INST")[4:8] + b"".join(slots))
    if anims != s.anims:
        rebuilt["IABL"] = chunk("IABL", struct.pack("<i", len(anims)) + s.data("IABL")[4:8] + b"".join(anims))
    if objects:
        rebuilt["OBJ0"] = chunk("OBJ0", struct.pack("<i", len(s.objects) + len(objects)) + s.data("OBJ0")[4:] +
                                b"".join(t.obj_bytes(k) for k in objects))
        notes.append(f"{len(objects)} objects from US v1.01 for instances {sorted(adopt.values())}")
    moved = sum(1 for k, x in enumerate(items) if x != k)
    if moved:
        notes.append(f"{moved} instances moved")
    body = bytearray()
    for tag in s.order:
        if tag in rebuilt:
            body += rebuilt[tag]
        else:
            src = t if tag in take else s
            off, size = src.chunks[tag]
            body += src.nu[off:off + size]
    out = s.nu[:4] + struct.pack("<i", -(16 + len(body))) + s.nu[8:16] + bytes(body)
    check_scene(out, t, {i: where[item_of(i)] for i in plats_new})
    return out, notes


def check_scene(nu: bytes, t: Scene, plats: dict[int, int]) -> None:
    s = Scene(nu)
    animated = sum(1 for r in s.instances if struct.unpack_from("<i", r, 0x48)[0] != 0)
    if animated != len(s.anims):
        raise Refused("animations do not match the instances")
    for r in s.instances:
        s.world(r)
    if any(i != k for i, k in plats.items()):
        raise Refused("a platform is not at its US v1.01 index")
    if any(not 0 <= i < len(s.instances) for _n, i in s.specials):
        raise Refused("a special past the instances")
    if [n for n, _i in s.specials] != [n for n, _i in t.specials] or [x[0] for x in s.splines] != \
            [x[0] for x in t.splines]:
        raise Refused("names differ from US v1.01")


def empty(b: bytes) -> bool:
    return len(b) <= 80 and not any(b[4:])


def tree(root: Path) -> dict[str, Path]:
    out = {}
    for p in sorted(root.rglob("*")):
        if p.is_file():
            out[p.relative_to(root).as_posix().upper()] = p
    return out


def kind(key: str) -> str:
    name = key.rsplit("/", 1)[-1]
    return name.rsplit(".", 1)[-1] if "." in name else ""


def stem(key: str) -> str:
    return key.rsplit(".", 1)[0]


def sha(b: bytes) -> str:
    return hashlib.sha256(b).hexdigest()


class Unpacked(dict):
    def __init__(self, raw: dict[str, bytes]):
        super().__init__()
        self.raw = raw

    def __missing__(self, key: str) -> bytes:
        value = unpack(self.raw[key])
        self[key] = value
        return value


def plan(src: dict[str, bytes], dst: Unpacked, log: list[str]) -> list[tuple[str, bytes | None, bytes, bool]]:
    files = sorted(dst.raw)
    old = Unpacked(src)
    differs = {k for k in files if k not in src or src[k] != dst.raw[k]}
    out = []
    refused = set()
    scenes = {}
    for k in files:
        if kind(k) != "GSC" or k not in src or k not in differs or old[k] == dst[k]:
            continue
        ter = stem(k) + ".TER"
        if ter not in dst.raw:
            continue
        try:
            patched, notes = scene_patch(old[k], dst[k], dst[ter], old[ter] if ter in src else None)
        except Refused as e:
            refused.add(stem(k))
            log.append(f"{stem(k)}: scene kept, {', '.join(sorted(COUPLED))} unpatched: {e}")
            continue
        scenes[k] = patched
        log.append(f"{stem(k)}: scene {'unchanged' if patched == old[k] else 'patched'}"
                   + (f" ({'; '.join(notes)})" if notes else ""))
    for k in files:
        if k not in differs or (stem(k) in refused and kind(k) in COUPLED):
            continue
        if k not in src:
            out.append((k, None, dst[k], False))
        elif kind(k) == "GSC":
            if k in scenes and scenes[k] != old[k]:
                out.append((k, src[k], scenes[k], True))
        elif old[k] != dst[k] and not (empty(old[k]) and empty(dst[k])):
            if dst.raw[k][:4] == b"RNC\x02":
                raise ValueError(f"{k}: packed on US v1.01")
            out.append((k, src[k], dst[k], False))
    return out


def level_files(root: Path) -> dict[str, bytes]:
    return {k: p.read_bytes() for k, p in tree(root).items() if k.startswith(LEVELS) and kind(k) in KINDS}


def generate(roots: dict[str, Path], log: list[str] | None = None) -> str:
    log = [] if log is None else log
    target = Unpacked(level_files(roots[TARGET]))
    words: list[int] = []
    segs: list[tuple[int, int, int]] = []
    rows = []
    seen: dict[tuple[str | None, str], tuple[int, int, int]] = {}
    for disc, _folder, manifest in SOURCES:
        if disc not in roots:
            continue
        src = level_files(roots[disc])
        dlog: list[str] = []
        for key, raw, result, scene in plan(src, target, dlog):
            old = sha(raw) if raw is not None else None
            ident = (old, sha(result))
            if ident not in seen:
                first, word = len(segs), len(words)
                base = unpack(raw) if raw is not None else b""
                edits = delta(base, result) if raw is not None else [(0, 0, result)]
                for o, n, lit in edits:
                    segs.append((o, n, len(lit)))
                    words += [w for (w,) in struct.iter_unpack("<I", lit + b"\0" * (-len(lit) % 4))]
                seen[ident] = (first, len(segs) - first, word)
                if rebuild(base, segs[first:], words[word:]) != result:
                    raise ValueError(f"{key}: the edits do not rebuild it")
            first, count, word = seen[ident]
            rows.append((manifest, key, old, sha(result), len(result), first, count, word, scene))
        log += [f"{disc}: {line}" for line in dlog]
    return table(words, segs, rows)


def rebuild(base: bytes, segs: list[tuple[int, int, int]], words: list[int]) -> bytes:
    out, w = bytearray(), 0
    for o, n, b in segs:
        if o + n > len(base):
            raise ValueError("copy past the source")
        out += base[o:o + n]
        k = (b + 3) // 4
        out += struct.pack(f"<{k}I", *words[w:w + k])[:b]
        w += k
    return bytes(out)


def table(words: list[int], segs: list[tuple[int, int, int]], rows: list[tuple]) -> str:
    lines = ["static const uint32_t nw_canon_words[] = {"]
    for i in range(0, len(words), 12):
        lines.append("    " + " ".join(f"0x{w:x}," for w in words[i:i + 12]))
    lines += ["    0", "};", "", "static const uint32_t nw_canon_segs[] = {"]
    for i in range(0, len(segs), 4):
        lines.append("    " + " ".join(f"{o}, {n}, {b}," for o, n, b in segs[i:i + 4]))
    lines += ["    0", "};", "", "static const NwCanonFile nw_canon_files[] = {"]
    for manifest, key, old, new, size, first, count, word, scene in rows:
        old_c = f'"{old}"' if old else "NULL"
        lines.append(f'    {{ &{manifest}, "{key}", {old_c}, "{new}", {size}u, {first}u, {count}u, {word}u, '
                     f'{"true" if scene else "false"} }},')
    lines += ["};", ""]
    return "\r\n".join(lines)


def roots_from(args: argparse.Namespace) -> dict[str, Path] | None:
    base = Path(args.data or os.environ.get("NUWORLD_DATA") or Path.home() / "NuWorldData")
    roots = {disc: Path(getattr(args, disc) or base / folder / "disc") for disc, folder, _m in SOURCES}
    roots[TARGET] = Path(args.usa101 or base / TARGET / "disc")
    return roots if all(p.is_dir() for p in roots.values()) else None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data")
    for disc, _folder, _m in SOURCES:
        parser.add_argument(f"--{disc}")
    parser.add_argument("--usa101")
    parser.add_argument("--out", type=Path, default=OUT)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    roots = roots_from(args)
    if roots is None:
        print("gen_canon: the four extracted discs are missing (--data or $NUWORLD_DATA): skipped")
        return SKIP
    log: list[str] = []
    text = generate(roots, log)
    if args.verbose:
        print("\n".join(log))
    if args.check:
        current = args.out.read_bytes().decode("utf-8") if args.out.is_file() else ""
        if current != text:
            print(f"gen_canon: {args.out} is stale (run tools/gen_canon.py)")
            return 1
        print(f"gen_canon: {args.out} is current")
        return 0
    args.out.write_bytes(text.encode("utf-8"))
    print(f"gen_canon: wrote {args.out} ({len(text)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
