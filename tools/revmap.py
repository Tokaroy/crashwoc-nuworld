#!/usr/bin/env python3
from __future__ import annotations

import argparse
import bisect
import difflib
import os
import struct
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "decomp" / "tools"))
from declib.elf import code_refs, read_elf  # noqa: E402

OUT = ROOT / "port" / "revmap"
REPORTS = ROOT / "build" / "revmap"
FUNC, OBJECT, NOTYPE = 2, 1, 0
STATIC_SECTIONS = (".sbss", ".bss", ".sdata", ".data")
JAL, LUI, JR_RA = 3, 0x0F, 0x03E00008
KEY = 8
SNAP = 6
ELFS = {
    "pal103": ("NUWORLD_PAL_ELF", "PAL v1.03"),
    "usa100": ("NUWORLD_US_ELF", "USA v1.00"),
    "pal201": ("NUWORLD_PAL201_ELF", "PAL v2.01"),
    "usa101": ("NUWORLD_US101_ELF", "USA v1.01"),
}
TARGETS = ("pal201", "usa101")
REFERENCE = "pal103"


def token(w: int) -> int:
    op = w >> 26
    if op in (2, JAL):
        return op << 26
    if op in (0, 0x1C) or op in (0x10, 0x11, 0x12) and (w >> 21 & 31) != 8:
        return w
    return w & 0xFFFF0000


@dataclass(frozen=True)
class Func:
    name: str
    addr: int
    size: int


@dataclass
class Image:
    base: int
    words: list[int]
    gp: int = 0
    funcs: list[Func] = field(default_factory=list)
    objects: list[tuple[int, int, str]] = field(default_factory=list)
    memory: list[tuple[int, bytes]] = field(default_factory=list)

    @classmethod
    def from_elf(cls, path: Path, stripped: bool = False) -> "Image":
        sections, symbols, _ = read_elf(path.read_bytes())
        by = {s["name"]: s for s in sections}
        text = by[".text"]
        data = text["data"][:len(text["data"]) // 4 * 4]
        words = list(struct.unpack(f"<{len(data) // 4}I", data))
        gp = struct.unpack_from("<I", by[".reginfo"]["data"], 20)[0]
        end = text["addr"] + len(data)
        funcs, objects, unsized = {}, [], []
        for s in [] if stripped else symbols:
            if s["type"] == FUNC and s["size"] and text["addr"] <= s["value"] < end:
                funcs.setdefault(s["value"], Func(s["name"], s["value"], s["size"]))
            elif s["type"] == OBJECT and s["size"]:
                objects.append((s["value"], s["size"], s["name"]))
            elif s["type"] in (NOTYPE, OBJECT) and not s["size"] and s["shndx"] < len(sections) and \
                    sections[s["shndx"]]["name"] in STATIC_SECTIONS:
                unsized.append((s["value"], sections[s["shndx"]], s["name"]))
        if unsized:
            marks = sorted({o[0] for o in objects} | {u[0] for u in unsized})
            for addr, sec, name in unsized:
                nxt = next((m for m in marks if m > addr), sec["addr"] + sec["size"])
                objects.append((addr, min(nxt, sec["addr"] + sec["size"]) - addr, name))
        memory = sorted((s["addr"], s["data"]) for s in sections if s["flags"] & 2 and s["data"] and s["addr"])
        return cls(text["addr"], words, gp, [funcs[a] for a in sorted(funcs)], sorted(set(objects)), memory)

    @property
    def end(self) -> int:
        return self.base + 4 * len(self.words)

    def code(self) -> bytes:
        return struct.pack(f"<{len(self.words)}I", *self.words)

    def tokens(self, addr: int, size: int) -> tuple[list[int], list[int]]:
        toks, at = [], []
        i0 = (addr - self.base) // 4
        for i in range(max(i0, 0), min(i0 + size // 4, len(self.words))):
            w = self.words[i]
            if w:
                toks.append(token(w))
                at.append(self.base + 4 * i)
        return toks, at

    def starts(self) -> set[int]:
        out, n = set(), len(self.words)
        for i, w in enumerate(self.words):
            if w >> 26 == JAL:
                t = (w & 0x3FFFFFF) << 2 | (self.base + 4 * i) & 0xF0000000
                if self.base <= t < self.end:
                    out.add(t)
            elif w == JR_RA:
                j = i + 2
                while j < n and self.words[j] == 0:
                    j += 1
                if j < n:
                    out.add(self.base + 4 * j)
        out.add(self.base + 4 * next((i for i, w in enumerate(self.words) if w), 0))
        return out

    def read(self, addr: int, n: int) -> bytes | None:
        for base, data in self.memory:
            if base <= addr and addr + n <= base + len(data):
                return data[addr - base:addr - base + n]
        return None

    def object_at(self, addr: int) -> tuple[int, int, str] | None:
        i = bisect.bisect_right(self.objects, (addr, 1 << 32, "")) - 1
        if i >= 0 and self.objects[i][0] <= addr < self.objects[i][0] + self.objects[i][1]:
            return self.objects[i]
        return None


@dataclass
class Placed:
    func: Func
    addr: int
    size: int
    ratio: float
    same: bool


@dataclass
class Mapping:
    placed: list[Placed]
    removed: list[Func]
    new: list[tuple[int, int]]
    data: list[tuple[int, int, str, int, int]]


def lis(pairs: list[tuple[int, int]]) -> list[tuple[int, int]]:
    tails, tail_at, prev = [], [], [-1] * len(pairs)
    for i, (_, p) in enumerate(pairs):
        j = bisect.bisect_left(tails, p)
        if j == len(tails):
            tails.append(p)
            tail_at.append(i)
        else:
            tails[j] = p
            tail_at[j] = i
        prev[i] = tail_at[j - 1] if j else -1
    out, i = [], tail_at[-1] if tail_at else -1
    while i >= 0:
        out.append(pairs[i])
        i = prev[i]
    return out[::-1]


def snap(addr: int, starts: list[int], lo: int, hi: int) -> int:
    i = bisect.bisect_left(starts, addr)
    best = None
    for s in starts[max(i - 2, 0):i + 2]:
        if lo <= s < hi and abs(s - addr) <= 4 * SNAP and (best is None or abs(s - addr) < abs(best - addr)):
            best = s
    return addr if best is None else best


def align(ref: Image, tgt: Image) -> Mapping:
    funcs = ref.funcs
    rtoks = [ref.tokens(f.addr, f.size)[0] for f in funcs]
    stok, saddr = tgt.tokens(tgt.base, 4 * len(tgt.words))
    index: dict[tuple, list[int]] = defaultdict(list)
    for p in range(len(stok) - KEY + 1):
        index[tuple(stok[p:p + KEY])].append(p)
    unique = []
    for k, tok in enumerate(rtoks):
        if len(tok) < KEY:
            continue
        cands = [p for p in index.get(tuple(tok[:KEY]), ()) if stok[p:p + len(tok)] == tok]
        if len(cands) == 1:
            unique.append((k, cands[0]))
    spans: dict[int, tuple[int, int, float]] = {}
    for k, p in lis(unique):
        spans[k] = (p, p + len(rtoks[k]), 1.0)
    taken = sorted((s, e) for s, e, _ in spans.values())
    moved = {}
    for k, p in unique:
        e = p + len(rtoks[k])
        i = bisect.bisect_right(taken, (p, 1 << 62))
        if k not in spans and (i == 0 or taken[i - 1][1] <= p) and (i == len(taken) or e <= taken[i][0]):
            moved[k] = (p, e, 1.0)
    hidden = list(stok)
    for s, e, _ in moved.values():
        hidden[s:e] = [-1] * (e - s)
    exact = set(spans) | set(moved)
    fences = [(-1, 0)] + [(k, spans[k][1]) for k in sorted(spans)] + [(len(funcs), len(stok))]
    for (ka, pa), (kb, pb) in zip(fences, fences[1:]):
        if kb - ka < 2 or pb <= pa:
            continue
        pb_start = spans[kb][0] if kb in spans else pb
        rseq, parts = [], []
        for k in range(ka + 1, kb):
            if k in moved:
                continue
            parts.append((k, len(rseq), len(rseq) + len(rtoks[k])))
            rseq += rtoks[k]
        tseq = hidden[pa:pb_start]
        sm = difflib.SequenceMatcher(None, rseq, tseq, autojunk=False)
        rmap = {}
        for blk in sm.get_matching_blocks():
            for d in range(blk.size):
                rmap[blk.a + d] = blk.b + d
        for k, s, e in parts:
            hits = [(r, rmap[r]) for r in range(s, e) if r in rmap]
            if e == s or len(hits) < (e - s if e - s <= 3 else max(2, (e - s) // 2)):
                continue
            r0, t0 = hits[0]
            r1, t1 = hits[-1]
            ts = pa + max(t0 - (r0 - s), 0)
            te = min(pa + t1 + 1 + (e - 1 - r1), pb_start)
            if te > ts:
                spans[k] = (ts, te, len(hits) / (e - s))
    spans.update(moved)
    starts = sorted(tgt.starts())
    order = sorted(spans, key=lambda k: spans[k][0])
    placed_at: list[tuple[int, int, int]] = []
    floor = tgt.base
    for n, k in enumerate(order):
        ts, te, _ = spans[k]
        nxt = saddr[spans[order[n + 1]][0]] if n + 1 < len(order) else tgt.end
        addr = saddr[ts] if k in exact else snap(saddr[ts], starts, floor, nxt)
        last = saddr[te - 1] + 4 if te - 1 < len(saddr) else tgt.end
        placed_at.append((k, addr, max(last, addr + 4)))
        floor = addr + 4
    ranges, new = regions(placed_at, starts, tgt.end)
    ratio = {k: spans[k][2] for k in ranges}
    rematch(rtoks, tgt, ranges, new, ratio, exact)
    ranges, new = regions(sorted(((k, a, l) for k, (a, _, l) in ranges.items()), key=lambda r: r[1]), starts,
                          tgt.end)
    placed = []
    for k, (addr, end, _) in ranges.items():
        same = rtoks[k] == tgt.tokens(addr, end - addr)[0]
        placed.append(Placed(funcs[k], addr, end - addr, ratio[k], same))
    removed = [f for k, f in enumerate(funcs) if k not in ranges]
    removed, new = follow_calls(ref, tgt, placed, removed, new)
    data = data_votes(ref, tgt, placed, sorted(set(starts) | {p.addr for p in placed}))
    data = data_pointers(ref, tgt, data)
    data = data_between(ref, tgt, one_to_one(ref, tgt, data_between(ref, tgt, data, free=True)))
    return Mapping(sorted(placed, key=lambda p: p.func.addr), removed, sorted(new), data)


def calls(img: Image, addr: int, size: int) -> list[int]:
    base = (addr - img.base) // 4
    return [(w & 0x3FFFFFF) << 2 | addr & 0xF0000000 for w in img.words[base:base + size // 4] if w >> 26 == 3]


def follow_calls(ref: Image, tgt: Image, placed: list[Placed], removed: list[Func],
                 new: list[tuple[int, int]]) -> tuple[list[Func], list[tuple[int, int]]]:
    """A function the aligner left out: an unclaimed target function that a placed caller calls at the same site."""
    lost = {f.addr: f for f in removed}
    free = dict(new)
    mapped = {p.func.addr: p.addr for p in placed}
    grew = True
    while grew and lost:
        grew = False
        for p in list(placed):
            rc, tc = calls(ref, p.func.addr, p.func.size), calls(tgt, p.addr, p.size)
            keys = [mapped.get(a, -1 - i) for i, a in enumerate(rc)]
            sites = []
            for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(None, keys, tc, autojunk=False).get_opcodes():
                if tag == "replace" and i2 - i1 == j2 - j1:
                    sites += zip(rc[i1:i2], tc[j1:j2])
            for a, b in sites:
                f = lost.get(a)
                if f is None or b not in free:
                    continue
                size = free.pop(b)
                rt, tt = ref.tokens(f.addr, f.size)[0], tgt.tokens(b, size)[0]
                placed.append(Placed(f, b, size, difflib.SequenceMatcher(None, rt, tt, autojunk=False).ratio(),
                                     rt == tt))
                mapped[a] = b
                del lost[a]
                grew = True
    return [f for f in removed if f.addr in lost], sorted(free.items())


def regions(placed_at: list[tuple[int, int, int]], starts: list[int], top: int):
    ranges, new = {}, []
    for n, (k, addr, last) in enumerate(placed_at):
        nxt = placed_at[n + 1][1] if n + 1 < len(placed_at) else top
        inner = [s for s in starts[bisect.bisect_left(starts, last):bisect.bisect_left(starts, nxt)] if s >= last]
        ranges[k] = (addr, inner[0] if inner else nxt, last)
        new += [(a, b - a) for a, b in zip(inner, inner[1:] + [nxt])]
    if placed_at:
        head = [s for s in starts if s < placed_at[0][1]]
        new += [(a, b - a) for a, b in zip(head, head[1:] + [placed_at[0][1]])]
    return ranges, sorted(new)


def rematch(rtoks, tgt: Image, ranges: dict, new: list, ratio: dict, exact: set) -> None:
    pool = {a: s for a, s in new if s >= 16}
    weak = [k for k in range(len(rtoks)) if k not in exact and len(rtoks[k]) >= KEY and ratio.get(k, 0.0) < 0.9]
    for k in sorted(weak, key=lambda k: ratio.get(k, 0.0)):
        rt = rtoks[k]
        cur = 0.0
        if k in ranges:
            a, e, _ = ranges[k]
            cur = difflib.SequenceMatcher(None, rt, tgt.tokens(a, e - a)[0], autojunk=False).ratio()
        best = None
        for a, s in pool.items():
            if not len(rt) * 2 <= s <= len(rt) * 8 + 64:
                continue
            sm = difflib.SequenceMatcher(None, rt, tgt.tokens(a, s)[0], autojunk=False)
            if sm.real_quick_ratio() <= cur + 0.1 or sm.quick_ratio() <= cur + 0.1:
                continue
            r = sm.ratio()
            if r >= 0.6 and r > cur + 0.1 and (best is None or r > best[0]):
                best = (r, a, s)
        if best:
            r, a, s = best
            del pool[a]
            if k in ranges:
                old, e, _ = ranges[k]
                if e - old >= 16:
                    pool[old] = e - old
            ranges[k] = (a, a + s, a + s)
            ratio[k] = r


def data_votes(ref: Image, tgt: Image, placed: list[Placed], tgt_starts: list[int]) -> list[tuple]:
    if not ref.objects:
        return []
    rrefs = {r.vram: r for r in code_refs(ref.code(), ref.base, [f.addr for f in ref.funcs], ref.gp)}
    trefs = {r.vram: r for r in code_refs(tgt.code(), tgt.base, tgt_starts, tgt.gp)}
    votes: dict[tuple[int, str], Counter] = defaultdict(Counter)
    for p in placed:
        rt, ra = ref.tokens(p.func.addr, p.func.size)
        tt, ta = tgt.tokens(p.addr, p.size)
        if rt == tt:
            pairs = list(zip(ra, ta))
        else:
            sm = difflib.SequenceMatcher(None, rt, tt, autojunk=False)
            pairs = [(ra[b.a + d], ta[b.b + d]) for b in sm.get_matching_blocks() for d in range(b.size)]
        for va, vb in pairs:
            a, b = rrefs.get(va), trefs.get(vb)
            if a is None or b is None or a.base != b.base:
                continue
            obj = ref.object_at(a.address)
            if obj:
                votes[(obj[0], obj[2])][b.address - (a.address - obj[0])] += 1
    out = []
    for (addr, name), c in sorted(votes.items()):
        best, n = c.most_common(1)[0]
        total = sum(c.values())
        if n * 4 >= total * 3:
            out.append((addr, best, name, n, total))
    return out


def alike(a: bytes | None, b: bytes | None) -> float:
    if not a or not b or len(a) != len(b):
        return 0.0
    n = len(a) // 4
    if n == 0:
        return 1.0 if a == b else 0.0
    same = sum(a[4 * i:4 * i + 4] == b[4 * i:4 * i + 4] for i in range(n))
    return same / n


def data_pointers(ref: Image, tgt: Image, voted: list[tuple]) -> list[tuple]:
    starts = {o[0]: o for o in ref.objects}
    placed = {a: b for a, b, _, _, _ in voted}
    out = list(voted)
    todo = list(placed.items())
    while todo:
        a, b = todo.pop()
        obj = starts.get(a)
        ra, rb = (ref.read(a, obj[1]), tgt.read(b, obj[1])) if obj else (None, None)
        if not ra or not rb:
            continue
        for off in range(0, len(ra) - 3, 4):
            x, y = int.from_bytes(ra[off:off + 4], "little"), int.from_bytes(rb[off:off + 4], "little")
            if x in starts and x not in placed and tgt.read(y, 4) is not None:
                placed[x] = y
                out.append((x, y, starts[x][2], 0, 0))
                todo.append((x, y))
    return sorted(out)


def one_to_one(ref: Image, tgt: Image, data: list[tuple]) -> list[tuple]:
    """One PAL object per target address: the one whose bytes the target holds there, then the most votes."""
    sizes = {o[0]: o[1] for o in ref.objects}
    claims = defaultdict(list)
    for t in data:
        claims[t[1]].append(t)

    def fit(t: tuple) -> tuple:
        size = sizes.get(t[0], 4)
        return alike(ref.read(t[0], size), tgt.read(t[1], size)), t[3], -t[0]
    return sorted(max(ts, key=fit) for ts in claims.values())


def named(body: bytes | None) -> bool:
    head = (body or b"")[:16].split(b"\0")[0]
    return len(head) >= 4 and all(32 <= c < 127 for c in head)


def data_between(ref: Image, tgt: Image, voted: list[tuple], free: bool = False) -> list[tuple]:
    """Objects next to placed ones at a neighbour's offset; with free, also onto taken addresses (for one_to_one)."""
    known = sorted((a, b) for a, b, _, _, _ in voted)
    have = {a for a, _ in known}
    taken = set() if free else {b for _, b in known}
    claim = (lambda b: None) if free else taken.add
    out = list(voted)
    for addr, size, name in ref.objects:
        if addr in have:
            continue
        i = bisect.bisect_left(known, (addr, 0))
        deltas = []
        if i > 0:
            deltas.append(known[i - 1][1] - known[i - 1][0])
        if i < len(known):
            deltas.append(known[i][1] - known[i][0])
        body = ref.read(addr, size)
        if body is None:
            if len(deltas) == 2 and deltas[0] == deltas[1] and addr + deltas[0] not in taken:
                out.append((addr, addr + deltas[0], name, 0, 0))
                claim(addr + deltas[0])
            continue
        best = max(((alike(body, tgt.read(addr + d, size)), d) for d in set(deltas) if addr + d not in taken),
                   default=(0.0, 0))
        if best[0] >= 0.75 and (len(set(deltas)) == 1 or best[0] == 1.0 or size >= 16):
            out.append((addr, addr + best[1], name, 0, 0))
            claim(addr + best[1])
        elif i > 0 and i < len(known) and named(body) and addr + deltas[0] not in taken and \
                addr + deltas[0] < known[i][1] and tgt.read(addr + deltas[0], 16) == body[:16]:
            out.append((addr, addr + deltas[0], name, 0, 0))
            claim(addr + deltas[0])
    return sorted(out)


class Addresses:
    def __init__(self, key: str, path: Path | None = None):
        self.names: dict[str, int] = {}
        pairs = []
        for line in (path or OUT / f"{key}.txt").read_text(encoding="utf-8").splitlines():
            p = line.split()
            if len(p) >= 4 and p[0] in ("func", "data"):
                a, b = int(p[1], 16), int(p[2], 16)
                self.names.setdefault(p[-1], b)
                if p[0] == "data":
                    pairs.append((a, b))
        self.pairs = sorted(pairs)

    def translate(self, pal: int) -> int:
        i = bisect.bisect_right(self.pairs, (pal, 1 << 32)) - 1
        if i < 0:
            raise KeyError(f"{pal:#x}: no mapped data before it")
        a, b = self.pairs[i]
        return pal - a + b


def lines(m: Mapping) -> list[str]:
    out = [f"func {p.func.addr:08x} {p.addr:08x} {p.size:x} {p.func.name}" for p in m.placed]
    out += [f"data {a:08x} {b:08x} {name}" for a, b, name, _, _ in m.data]
    return out


def report(m: Mapping, ref: Image, tgt_name: str) -> list[str]:
    same = sum(p.same for p in m.placed)
    out = [f"{tgt_name}: {len(m.placed)} of {len(ref.funcs)} functions placed ({same} token-identical, "
           f"{len(m.placed) - same} changed), {len(m.removed)} not found, {len(m.new)} new, "
           f"{len(m.data)} of {len(ref.objects)} data objects"]
    out += [f"changed {p.func.addr:08x} -> {p.addr:08x} {p.size:#x} {p.ratio:.2f} {p.func.name}"
            for p in m.placed if not p.same]
    out += [f"missing {f.addr:08x} {f.size:#x} {f.name}" for f in m.removed]
    out += [f"new {a:08x} {s:#x}" for a, s in m.new]
    return out


def score(m: Mapping, truth: Image) -> tuple[int, int, int]:
    real = defaultdict(list)
    for f in truth.funcs:
        real[f.name].append(f.addr)
    right = sum(p.addr in real.get(p.func.name, ()) for p in m.placed)
    named = sum(p.func.name in real for p in m.placed)
    return right, named, len(m.placed)


def elf_path(args, key: str) -> Path:
    env, what = ELFS[key]
    value = getattr(args, key) or os.environ.get(env)
    if not value:
        sys.exit(f"error: the {what} ELF: --{key} or ${env}")
    return Path(value)


def build(args, key: str) -> Mapping:
    ref = Image.from_elf(elf_path(args, REFERENCE))
    return align(ref, Image.from_elf(elf_path(args, key), stripped=True)), ref


def cmd_map(args) -> int:
    bad = 0
    for key in args.target or TARGETS:
        m, _ = build(args, key)
        text = "\n".join(lines(m)) + "\n"
        path = OUT / f"{key}.txt"
        if args.check:
            have = path.read_text(encoding="utf-8").replace("\r\n", "\n") if path.exists() else ""
            if have != text:
                print(f"{path.relative_to(ROOT)}: differs from a fresh map; run tools/revmap.py map")
                bad += 1
            else:
                print(f"{path.relative_to(ROOT)}: as mapped ({len(m.placed)} functions, {len(m.data)} data)")
        else:
            OUT.mkdir(parents=True, exist_ok=True)
            path.write_bytes(text.encode("utf-8"))
            print(f"{path.relative_to(ROOT)}: {len(m.placed)} functions, {len(m.data)} data")
    return 1 if bad else 0


def cmd_report(args) -> int:
    REPORTS.mkdir(parents=True, exist_ok=True)
    for key in args.target or TARGETS:
        m, ref = build(args, key)
        out = report(m, ref, ELFS[key][1])
        (REPORTS / f"{key}.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
        print(out[0])
    return 0


def cmd_selftest(args) -> int:
    pal = elf_path(args, "pal103")
    us = elf_path(args, "usa100")
    worst = 1.0
    for ref_path, tgt_path, label in ((us, pal, "USA v1.00 -> PAL v1.03"), (pal, us, "PAL v1.03 -> USA v1.00")):
        ref = Image.from_elf(ref_path)
        truth = Image.from_elf(tgt_path)
        stripped = Image.from_elf(tgt_path, stripped=True)
        m = align(ref, stripped)
        right, named, total = score(m, truth)
        data_right = sum(b == next((o[0] for o in truth.objects if o[2] == name), None) for _, b, name, _, _ in m.data)
        print(f"{label}: {right} of {named} named functions at their real address ({total} placed), "
              f"{data_right} of {len(m.data)} data objects")
        worst = min(worst, right / max(named, 1))
    return 0 if worst >= args.min else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Name the functions and data of the stripped PAL v2.01 and USA v1.01 "
                                                 "boot ELFs by aligning them with the PAL v1.03 one.")
    sub = parser.add_subparsers(dest="cmd", required=True)
    for name in ("map", "report", "selftest"):
        p = sub.add_parser(name)
        for key, (env, what) in ELFS.items():
            p.add_argument(f"--{key}", help=f"the {what} ELF (default ${env})")
        if name != "selftest":
            p.add_argument("--target", action="append", choices=TARGETS)
    sub.choices["map"].add_argument("--check", action="store_true")
    sub.choices["selftest"].add_argument("--min", type=float, default=0.98)
    args = parser.parse_args()
    return {"map": cmd_map, "report": cmd_report, "selftest": cmd_selftest}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
