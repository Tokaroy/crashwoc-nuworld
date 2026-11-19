#!/usr/bin/env python3
from __future__ import annotations

import argparse
import difflib
import math
import os
import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ntsc_diff as nd  # noqa: E402
import nwunits  # noqa: E402
import patches  # noqa: E402
import revmap  # noqa: E402

ROOT = nd.ROOT
OUT = ROOT / "build" / "rev_diff"
KNOWN = ROOT / "tools" / "rev_diff_known.txt"
MODES = {"plain": [], "late": ["-DNW_REV_LATEST"],
         "late_ntsc": ["-DNW_REV_LATEST", "-DNW_HZ_NTSC", "-DNW_REV_US101_LAYOUT"]}
BUILD_OF = {"pal201": "late", "usa101": "late_ntsc"}
LINE_CALLS = re.compile(r"(NuErrorProlog|NuWarningProlog|NuDebugMsgProlog|NuMem(Alloc|ReAlloc|Free)Fn)$")
R_MIPS_26 = 4


def elf_path(key: str, value: str | None = None) -> Path:
    env, what = revmap.ELFS[key]
    path = value or os.environ.get(env)
    if not path:
        sys.exit(f"error: the {what} ELF: --{key} or ${env}")
    return Path(path)


class Target(nd.Elf):
    def __init__(self, key: str, path: Path, ref: nd.Elf):
        sizes = {o[0]: o[1] - o[0] for o in ref.objects}
        funcs, objects = [], []
        for line in (revmap.OUT / f"{key}.txt").read_text(encoding="utf-8").splitlines():
            p = line.split()
            if p and p[0] == "func":
                funcs.append({"name": p[4], "value": int(p[2], 16), "size": int(p[3], 16), "ref": int(p[1], 16)})
            elif p and p[0] == "data":
                ref_addr, addr = int(p[1], 16), int(p[2], 16)
                objects.append((addr, addr + sizes.get(ref_addr, 4), p[3]))
        super().__init__(path, funcs, objects)
        self.key = key
        self.by_ref = {f["ref"]: f for f in funcs}
        self.data_ref = {}
        for line in (revmap.OUT / f"{key}.txt").read_text(encoding="utf-8").splitlines():
            p = line.split()
            if p and p[0] == "data":
                self.data_ref[int(p[1], 16)] = (int(p[2], 16), p[3])


def callees_elf(elf: nd.Elf, f: dict) -> dict[int, str]:
    out = {}
    base = f["value"] - elf.text["addr"]
    for i in range(f["size"] // 4):
        w = int.from_bytes(elf.text["data"][base + 4 * i:base + 4 * i + 4], "little")
        if w >> 26 == 3:
            name = elf.func_at.get((w & 0x3FFFFFF) << 2 | f["value"] & 0xF0000000)
            if name:
                out[4 * i] = name
    return out


def callees_obj(obj: nd.Obj, f: dict) -> dict[int, str]:
    out = {}
    for i in range(f["size"] // 4):
        t, sym = obj.relocs.get(f["value"] + 4 * i, (None, None))
        if t == R_MIPS_26:
            out[4 * i] = obj.symbols[sym]["name"]
    return out


def line_loads(code: nd.Code, callees: dict[int, str]) -> set[int]:
    calls = sorted(off for off, name in callees.items() if LINE_CALLS.search(name))
    out = set()
    for i, w in enumerate(code.words):
        if w >> 26 == 9 and w >> 21 & 31 == 0 and 4 <= (w >> 16 & 31) <= 7:
            off = code.offs[i]
            if any(off < c <= off + 16 for c in calls):
                out.add(i)
    return out


def render(words: list[int], vram: int, names: dict[int, str]) -> list[str]:
    import rabbitizer
    out = []
    for i, w in enumerate(words):
        if w == 0:
            continue
        text = rabbitizer.Instruction(w, vram + 4 * i, rabbitizer.InstrCategory.R5900).disassemble()
        if i in names and w >> 26 not in (2, 3):
            text = re.sub(r"-?0x[0-9A-F]+(?!.*0x)", names[i], text)
        text = re.sub(r"func_[0-9A-F]+", "func", text)
        text = re.sub(r"-?0x[0-9A-F]+\(\$gp\)", "gp", text)
        out.append(re.sub(r"\. \+ 4 \+ \(-?0x[0-9A-F]+ << 2\)", "target", text))
    return out


def listing(elf: nd.Elf, f: dict, named: bool = True) -> list[str]:
    base = f["value"] - elf.text["addr"]
    raw = elf.text["data"][base:base + f["size"]]
    words = [int.from_bytes(raw[k:k + 4], "little") for k in range(0, len(raw), 4)]
    names = {}
    for i in range(len(words)):
        vram = f["value"] + 4 * i
        if vram in elf.lo or vram in elf.hi:
            r = elf.lo.get(vram)
            at = elf.target(r.address) if r and named else None
            names[i] = f"{at[0]}+{at[1]:#x}" if at else "addr"
    return render(words, f["value"], names)


def listing_obj(obj: nd.Obj, f: dict) -> list[str]:
    raw = obj.text["data"][f["value"]:f["value"] + f["size"]]
    words = [int.from_bytes(raw[k:k + 4], "little") for k in range(0, len(raw), 4)]
    names = {i: "addr" for i in range(len(words)) if f["value"] + 4 * i in obj.relocs}
    return render(words, 0, names)


def sound_ids(ref: nd.Elf, tgt: Target) -> set[tuple[int, int]]:
    """(PAL v1.03's number, the target's) of each sound both executables' tables name alike. A sound's number is
    its row in SfxTabGLOBAL, or the global rows plus its row in its level's table; PAL's extra language rows give the
    same sound another number."""
    starts = sorted({b for b, _ in tgt.data_ref.values()})
    tables = {}
    for lo, hi, name in ref.objects:
        if not name.startswith("SfxTab") or lo not in tgt.data_ref:
            continue
        b = tgt.data_ref[lo][0]
        end = next((s for s in starts if s > b), b + hi - lo)
        a, c = ref.read(lo, hi - lo), tgt.read(b, min(end - b, hi - lo))
        if a is not None and c is not None:
            tables[name] = [[t[off:off + 16].split(b"\0")[0] for off in range(0, len(t) - nd.SOUND + 1, nd.SOUND)]
                            for t in (a, c)]
    if "SfxTabGLOBAL" not in tables:
        return set()
    base = [len(rows) for rows in tables["SfxTabGLOBAL"]]
    out = set()
    for name, (ours, theirs) in tables.items():
        first = {}
        for row, sound in enumerate(theirs):
            first.setdefault(sound, row)
        add = (0, 0) if name == "SfxTabGLOBAL" else base
        for row, sound in enumerate(ours):
            if sound in first and row + add[0] != first[sound] + add[1]:
                out.add((row + add[0], first[sound] + add[1]))
    return out


def report(d: nd.Diff, key: str, known: nd.Known, out: list[str], counts: Counter,
           sounds: set[tuple[int, int]] = frozenset()) -> bool:
    left, lines = False, []
    base = f"func:{d.unit}/{d.name}@{key}"
    if d.unaligned:
        why = known.reason(f"{base} structure")
        counts["structure known" if why else "structure"] += 1
        if not why:
            lines.append(f"  structure: {d.unaligned} instructions differ")
            left = True
    for s in d.sites:
        kind = s.pal.kind if s.pal else "?"
        text = f"{kind} {s.pal.show() if s.pal else '-'} -> {s.ntsc.show() if s.ntsc else '-'}"
        cls = s.cls
        if kind == "imm" and s.ntsc and (s.pal.bits, s.ntsc.bits) in sounds:
            cls = "sound"
        why = known.reason(f"{base}+0x{s.off:x} {text}")
        counts[f"{cls} known" if why else cls] += 1
        if why or cls == "sound":
            continue
        left = True
        lines.append(f"  +0x{s.off:04x} {text}  {cls}")
    if lines:
        out.append(f"{d.unit or '?'} {d.name}@{key}:")
        out.extend(lines)
    return left


def diff(a: nd.Code, b: nd.Code, lines_a: set[int]) -> nd.Diff:
    d = nd.compare(a, b)
    d.sites = [s for s in d.sites if s.index not in lines_a]
    return d


def ref_elf(args) -> nd.Elf:
    return nd.Elf(elf_path("pal103", args.pal103))


def targets(args, ref: nd.Elf) -> dict[str, Target]:
    keys = args.target or list(BUILD_OF)
    return {k: Target(k, elf_path(k, getattr(args, k)), ref) for k in keys}


def cmd_functions(args) -> int:
    ref = ref_elf(args)
    units = nd.unit_of(nwunits.decomp_tree(args.decomp))
    known, rc = nd.Known(args.known), 0
    for key, tgt in targets(args, ref).items():
        counts, out = Counter(), []
        if args.disasm:
            for f in ref.funcs:
                g = tgt.by_ref.get(f["value"])
                if f["name"] == args.disasm and g:
                    a = listing(ref, f)
                    b = listing(tgt, g)
                    for line in difflib.unified_diff(a, b, "PAL v1.03", revmap.ELFS[key][1], lineterm="", n=3):
                        print(line)
            continue
        for f in ref.funcs:
            g = tgt.by_ref.get(f["value"])
            if not g:
                continue
            code = ref.code(f, units.get(f["value"], ""))
            d = diff(code, tgt.code(g), line_loads(code, callees_elf(ref, f)))
            if d.sites or d.unaligned:
                report(d, key, known, out, counts)
        print("\n".join(out))
        print(f"{revmap.ELFS[key][1]}: " + ", ".join(f"{n} {k}" for k, n in sorted(counts.items())))
    return rc


REV_VALUE = re.compile(r'\{ "[^"]+", (0x[0-9a-fA-F]+)u, (\d+), (\w+?)u, (\w+?)u, (\w+?)u \}')
REV_STRINGS = re.compile(r'static const char \*const (\w+)\[\d+\] = \{([^}]*)\}')
REV_STRING_USE = re.compile(r'(\w+)\[(\d+)\]\[i\] = \(char \*\)(\w+)\[i\];')
TEXT_TABLE = re.compile(r'^char \*(\w+)((?:\[\d+\])+) =', re.M)
CREDIT_ROW = re.compile(r'\{ (?:"([^"]*)"|0), (-?\d+), (-?\d+) \}')


def text_tables(decomp: Path) -> dict[str, list[int]]:
    """The dimensions of each language table in game/text."""
    text = (decomp / "src" / "game" / "text.c").read_text(encoding="latin-1")
    return {m[1]: [int(d) for d in re.findall(r"\d+", m[2])] for m in TEXT_TABLE.finditer(text)}


def english(dims: list[int] | None, off: int) -> bool:
    """Whether a table's pointer at off is its English text: the language is the last index when it has 6,
    else the first."""
    if not dims or 6 not in (dims[0], dims[-1]):
        return True
    i = off // 4
    if dims[-1] == 6:
        return i % 6 == 0
    return i < math.prod(dims[1:])


def data_units(decomp: Path) -> dict[str, str]:
    """PAL v1.03's data symbols of the units NuWorld compiles, with their unit."""
    import tomllib
    compiled = {u.index: u.name for u in nwunits.load(decomp).units if u.treatment == "compile"}
    doc = tomllib.loads((decomp / "config" / "pal103" / "data_symbols.toml").read_text(encoding="utf-8"))
    return {s["name"]: compiled[s["unit"]] for s in doc["symbol"] if s.get("unit") in compiled}


def credit_rows(decomp: Path) -> list[tuple[bytes | None, int, int]]:
    """CreditLate, the late credits NuWorld shows (game/credits)."""
    text = (decomp / "src" / "game" / "credits.c").read_text(encoding="latin-1")
    if "CreditLate[" not in text:
        return []
    body = text[text.index("CreditLate["):]
    body = body[:body.index("};")]
    return [(m[1].encode("latin-1") if m[1] is not None else None, int(m[2]), int(m[3]))
            for m in CREDIT_ROW.finditer(body)]


class Late:
    """PAL v1.03's data as NuWorld's late game holds it: src/port/rev.c's values and strings over it, and at
    60 Hz the 60 Hz table (src/port/hz_table.inc)."""

    def __init__(self, ref: nd.Elf, shapes: dict[str, list[int]], ntsc: bool):
        self.ref = ref
        self.patch: dict[int, int] = {}
        self.strings: dict[int, bytes] = {}
        text = (ROOT / "src" / "port" / "rev.c").read_text(encoding="utf-8")
        for m in REV_VALUE.finditer(text):
            addr, size, value = int(m[1], 16), int(m[2]), int(m[5 if ntsc else 4], 0)
            for k in range(size):
                self.patch[addr + k] = value >> 8 * k & 0xFF
        if ntsc:
            for _, addr, size, _, value in hz_rows(ROOT / "src" / "port" / "hz_table.inc"):
                for k in range(size):
                    self.patch[addr + k] = value[k]
        arrays = {m[1]: re.findall(r'"([^"]*)"', m[2]) for m in REV_STRINGS.finditer(text)}
        starts = {name: lo for lo, _, name in ref.objects}
        for m in REV_STRING_USE.finditer(text):
            row = 4 * (shapes.get(m[1], [0, 1])[-1])
            for i, s in enumerate(arrays.get(m[3], [])):
                self.strings[starts[m[1]] + int(m[2]) * row + 4 * i] = s.encode("latin-1")

    def read(self, addr: int, n: int) -> bytes | None:
        raw = self.ref.read(addr, n)
        if raw is None:
            return None
        out = bytearray(raw)
        for k in range(n):
            if addr + k in self.patch:
                out[k] = self.patch[addr + k]
        return bytes(out)


def same_text(ref: nd.Elf, tgt: Target, x: int, y: int) -> bool:
    sx, sy = ref.section_at(x), tgt.section_at(y)
    if not sx or not sy:
        return False
    a, b = nd.cstr(ref, x), nd.cstr(tgt, y)
    return a == b and (bool(a) or sx["name"] == sy["name"])


def pointer(elf: nd.Elf, v: int) -> bool:
    return elf.section_at(v) is not None or elf.target(v) is not None


def pointer_class(late: Late, tgt: Target, name: str, addr: int, at: int, x: int, y: int,
                  shapes: dict[str, list[int]]) -> str | None:
    """None when both point at the same thing: the same symbol, equal text, or the late text NuWorld puts there."""
    ours = late.strings.get(addr)
    if ours is not None:
        return None if ours == nd.cstr(tgt, y) else "review"
    tx = late.ref.target(x)
    if (tx is not None and tx == tgt.target(y)) or same_text(late.ref, tgt, x, y):
        return None
    if name in shapes and not english(shapes[name], at):
        return "language"
    return "review"


def credit_changes(tgt: Target, at: int, rows: list[tuple]) -> list[tuple[nd.Field, str]]:
    raw = tgt.read(at, 8 * len(rows)) or b""
    out = []
    for i, (txt, colour, size) in enumerate(rows):
        if 8 * i + 8 > len(raw):
            break
        p, c, s = struct.unpack_from("<Ihh", raw, 8 * i)
        theirs = nd.cstr(tgt, p) if p else None
        if (theirs, c, s) != (txt, colour, size):
            what = f"{txt!r} {colour} {size} -> {theirs!r} {c} {s}"
            out.append((nd.Field("Credit", 8 * i, 8, "row", 0, 0, what, 0), "review"))
    return out


def data_changes(late: Late, tgt: Target, units: dict[str, str], shapes: dict[str, list[int]],
                 credits: list[tuple], sounds: set[tuple[int, int]]) -> list[tuple[nd.Field, str]]:
    """Each value of NuWorld's late data that differs from the target's, with its class: review, rate, sound
    (the same sound under the target's number) or language (a text PAL keeps in its other languages)."""
    ref = late.ref
    starts = sorted({b for b, _ in tgt.data_ref.values()})
    out = []
    for lo, hi, name in ref.objects:
        sec = ref.section_at(lo)
        if name not in units or lo not in tgt.data_ref or not sec or sec["name"] not in nd.DATA_SECTIONS:
            continue
        at = tgt.data_ref[lo][0]
        if name == "Credit" and credits:
            out += credit_changes(tgt, at, credits)
            continue
        a = late.read(lo, hi - lo)
        end = next((s for s in starts if s > at), at + hi - lo)
        b = tgt.read(at, min(end - at, hi - lo))
        if name.startswith("SfxTab"):
            pairs = nd.sound_pairs(a, b) if a and b else []
        else:
            pairs = [(0, a[:len(b)], b)] if a and b else []
        for base, ea, eb in pairs:
            for off in range(0, len(ea) - len(ea) % 4, 4):
                x, y = int.from_bytes(ea[off:off + 4], "little"), int.from_bytes(eb[off:off + 4], "little")
                if (x == y and lo + base + off not in late.strings) or tgt.data_ref.get(x, (None,))[0] == y:
                    continue
                if pointer(ref, x) and pointer(tgt, y):
                    cls = pointer_class(late, tgt, name, lo + base + off, base + off, x, y, shapes)
                    if cls:
                        ours = late.strings.get(lo + base + off, nd.cstr(ref, x))
                        what = f"{ours[:40]!r} -> {nd.cstr(tgt, y)[:40]!r}"
                        out.append((nd.Field(name, base + off, 4, "ptr", x, y, what, lo), cls))
                    continue
                for f in nd.data_fields(name, ea[off:off + 4], eb[off:off + 4], ref, tgt):
                    f.off += base + off
                    f.addr = lo
                    out.append((f, "sound" if f.kind == "int" and (f.pal, f.ntsc) in sounds else f.cls))
    return out


def cmd_data(args) -> int:
    ref = ref_elf(args)
    decomp = nwunits.decomp_tree(args.decomp)
    units, shapes, credits = data_units(decomp), text_tables(decomp), credit_rows(decomp)
    known, counts, out, left = nd.Known(args.known), Counter(), [], False
    for key, tgt in targets(args, ref).items():
        late = Late(ref, shapes, BUILD_OF[key] == "late_ntsc")
        for f, cls in data_changes(late, tgt, units, shapes, credits, sound_ids(ref, tgt)):
            what = f.cls if f.kind in ("ptr", "row") else f"{f.show(f.pal)} -> {f.show(f.ntsc)}"
            line = f"{f.symbol}@{key}+0x{f.off:x} {f.kind}{f.size} {what}"
            why = known.reason(f"data:{line}")
            counts[f"{cls} known" if why else cls] += 1
            if why or cls in ("sound", "language"):
                continue
            out.append(f"  {line}  {cls}")
            left = True
    print("\n".join(out))
    print(", ".join(f"{n} {k}" for k, n in sorted(counts.items())) or "nothing differs")
    return 1 if left else 0


HZ_ROW = re.compile(r'NW_HZ_(INT|F32)\("([^"]+)", 0x([0-9a-f]+), (?:(\d+), (-?\d+), (-?\d+)|([-+0-9.e]+)f, ([-+0-9.e]+)f)\)')


def hz_rows(path: Path) -> list[tuple[str, int, int, bytes, bytes]]:
    out = []
    for m in HZ_ROW.finditer(path.read_text(encoding="utf-8")):
        kind, name, addr = m[1], m[2], int(m[3], 16)
        if kind == "INT":
            size = int(m[4])
            pal, ntsc = (int(v) & ((1 << 8 * size) - 1) for v in (m[5], m[6]))
            out.append((name, addr, size, pal.to_bytes(size, "little"), ntsc.to_bytes(size, "little")))
        else:
            out.append((name, addr, 4, struct.pack("<f", float(m[7])), struct.pack("<f", float(m[8]))))
    return out


def cmd_hz(args) -> int:
    ref = ref_elf(args)
    tgt = targets(argparse.Namespace(target=["usa101"], usa101=args.usa101), ref)["usa101"]
    rows = hz_rows(ROOT / "src" / "port" / "hz_table.inc")
    bad = Counter()
    for name, addr, size, pal, ntsc in rows:
        obj = next((o for o in ref.objects if o[0] <= addr < o[1]), None)
        mapped = tgt.data_ref.get(obj[0]) if obj else None
        have = tgt.read(mapped[0] + addr - obj[0], size) if mapped else None
        if have != ntsc:
            bad[name.split("+")[0]] += 1
            if args.verbose:
                print(f"{name}: table {ntsc.hex()}, USA v1.01 {have.hex() if have else 'unmapped'}")
    print(f"{len(rows)} 60 Hz values, {len(rows) - sum(bad.values())} as in USA v1.01" +
          ("; differ: " + ", ".join(f"{k} {n}" for k, n in bad.most_common()) if bad else ""))
    return 1 if bad else 0


def in_container() -> bool:
    return Path("/.dockerenv").exists()


def cmd_compile(args) -> int:
    patches.link_compiler(args.decomp)
    sys.path.insert(0, str(args.decomp / "tools"))
    import cc
    for unit in args.units:
        src = args.decomp / "src" / f"{unit}.c"
        for mode, extra in MODES.items():
            dest = args.out / mode / f"{unit}.o"
            try:
                cc._run_driver(src, "out.o", ["-c", *extra], dest, "default", nd.VERSION)
            except subprocess.CalledProcessError as e:
                sys.exit(f"{unit} ({mode}): {e.stderr}")
    return 0


def compile_units(decomp: Path, units: list[str]) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    if in_container():
        cmd_compile(argparse.Namespace(decomp=decomp, out=OUT, units=units))
        return
    cmd = ["docker", "run", "--rm", "-v", f"{ROOT}:/nw", "-v", f"{decomp.resolve()}:/decomp",
           "-v", f"{(nd.DECOMP / 'compiler').resolve()}:/decomp/compiler", "-w", "/nw", nd.IMAGE,
           "python3", "tools/rev_diff.py", "compile", "--decomp", "/decomp", "--out", "build/rev_diff", *units]
    if subprocess.run(cmd, stdin=subprocess.DEVNULL).returncode:
        sys.exit("error: the EE compile failed")


def not_run(decomp: Path) -> set[tuple[str, str]]:
    """(unit, function) of the retail code NuWorld never runs: a NuWorld definition, a stub or nothing takes its
    calls (port/units.toml); a wrapped function's retail body still runs."""
    model = nwunits.load(decomp)
    return {(u.name, re.sub(r"__[0-9a-f]{8}$", "", f.name)) for u in model.units for f in u.functions
            if f.provider != "decomp" and not f.wrapped}


def cmd_check(args) -> int:
    ref = ref_elf(args)
    decomp = nwunits.decomp_tree(args.decomp)
    units = nd.unit_of(decomp)
    skip = not_run(decomp)
    tgts = targets(args, ref)
    sounds = {key: sound_ids(ref, tgt) for key, tgt in tgts.items()}
    by_key = {(units.get(f["value"], ""), re.sub(r"__[0-9a-f]{8}$", "", f["name"])): f for f in ref.funcs}
    wanted = set(args.functions or [])
    todo = sorted(args.units or {units[f["value"]] for f in ref.funcs if f["name"] in wanted
                                 and units.get(f["value"])})
    todo = [u for u in todo if (decomp / "src" / f"{u}.c").is_file()]
    if not todo:
        sys.exit("error: no units: --units or --functions")
    if not args.no_compile:
        compile_units(decomp, todo)
    known, counts, out, left = nd.Known(args.known), Counter(), [], False
    for unit in todo:
        objs = {mode: nd.Obj(OUT / mode / f"{unit}.o", unit, ref) for mode in MODES}
        for f in objs["plain"].funcs:
            code = objs["plain"].code(f)
            r = by_key.get((unit, code.name))
            if r is None or (wanted and code.name not in wanted) or (unit, code.name) in skip:
                continue
            d = diff(code, ref.code(r), line_loads(code, callees_obj(objs["plain"], f)))
            if d.sites or d.unaligned:
                out.append(f"{unit} {code.name}: the plain build is not PAL v1.03's ({len(d.sites)} values, "
                           f"{d.unaligned} instructions)")
                left = True
        for key, tgt in tgts.items():
            obj = objs[BUILD_OF[key]]
            for f in obj.funcs:
                code = obj.code(f)
                r = by_key.get((unit, code.name))
                if r is None or (wanted and code.name not in wanted):
                    continue
                if (unit, code.name) in skip:
                    counts["not run"] += 1
                    continue
                g = tgt.by_ref.get(r["value"])
                if g is None:
                    why = known.reason(f"func:{unit}/{code.name}@{key} missing")
                    counts["missing known" if why else "missing"] += 1
                    if not why:
                        out.append(f"{unit} {code.name}: not in {revmap.ELFS[key][1]}")
                        left = True
                    continue
                d = diff(code, tgt.code(g), line_loads(code, callees_obj(obj, f)))
                if args.disasm and (d.sites or d.unaligned):
                    out += difflib.unified_diff(listing_obj(obj, f), listing(tgt, g, False), "built",
                                                revmap.ELFS[key][1], lineterm="", n=2)
                counts["functions"] += 1
                if d.sites or d.unaligned:
                    left |= report(d, key, known, out, counts, sounds[key])
                else:
                    counts["equal"] += 1
    print("\n".join(out))
    print(", ".join(f"{n} {k}" for k, n in sorted(counts.items())) or "nothing compared")
    return 1 if left else 0


def main() -> int:
    parser = argparse.ArgumentParser(description="The decomp built with the late revision's NW_REV values against the "
                                                 "PAL v2.01 and USA v1.01 code, through port/revmap.")
    sub = parser.add_subparsers(dest="cmd", required=True)
    for name in ("functions", "check", "data"):
        p = sub.add_parser(name)
        for key, (env, what) in revmap.ELFS.items():
            p.add_argument(f"--{key}", help=f"the {what} ELF (default ${env})")
        p.add_argument("--target", action="append", choices=list(BUILD_OF))
        p.add_argument("--known", type=Path, default=KNOWN)
        p.add_argument("--decomp", type=Path, default=None)
    sub.choices["functions"].add_argument("--disasm", metavar="NAME")
    sub.choices["check"].add_argument("--units", nargs="*")
    sub.choices["check"].add_argument("--functions", nargs="*")
    sub.choices["check"].add_argument("--no-compile", action="store_true")
    sub.choices["check"].add_argument("--disasm", action="store_true")
    p = sub.add_parser("hz")
    p.add_argument("--pal103")
    p.add_argument("--usa101")
    p.add_argument("--verbose", action="store_true")
    p = sub.add_parser("compile", help=argparse.SUPPRESS)
    p.add_argument("--decomp", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("units", nargs="+")
    args = parser.parse_args()
    return {"functions": cmd_functions, "check": cmd_check, "data": cmd_data, "hz": cmd_hz, "compile": cmd_compile}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
