#!/usr/bin/env python3
from __future__ import annotations

import argparse
import difflib
import os
import re
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
MODES = {"plain": [], "late": ["-DNW_REV_LATEST"], "late_ntsc": ["-DNW_REV_LATEST", "-DNW_HZ_NTSC"]}
BUILD_OF = {"pal201": "late", "usa101": "late_ntsc"}
LINE_CALLS = re.compile(r"(NuErrorProlog|NuWarningProlog|NuDebugMsgProlog|NuMem(Alloc|ReAlloc|Free)Fn)$")
R_MIPS_26 = 4
LAYOUT = (8, 16)


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


def site_class(s: nd.Site, key: str) -> str:
    if key == "usa101" and s.pal and s.ntsc and s.pal.kind == s.ntsc.kind and s.pal.kind in ("off", "imm"):
        if s.pal.bits - s.ntsc.bits in LAYOUT:
            return "layout"
    return s.cls


def report(d: nd.Diff, key: str, known: nd.Known, out: list[str], counts: Counter) -> bool:
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
        cls = site_class(s, key)
        why = known.reason(f"{base}+0x{s.off:x} {text}")
        counts[f"{cls} known" if why else cls] += 1
        if why or cls == "layout":
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


def cmd_check(args) -> int:
    ref = ref_elf(args)
    decomp = nwunits.decomp_tree(args.decomp)
    units = nd.unit_of(decomp)
    tgts = targets(args, ref)
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
            if r is None or (wanted and code.name not in wanted):
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
                g = tgt.by_ref.get(r["value"])
                if g is None:
                    out.append(f"{unit} {code.name}: not in {revmap.ELFS[key][1]}")
                    continue
                d = diff(code, tgt.code(g), line_loads(code, callees_obj(obj, f)))
                if args.disasm and (d.sites or d.unaligned):
                    out += difflib.unified_diff(listing_obj(obj, f), listing(tgt, g, False), "built",
                                                revmap.ELFS[key][1], lineterm="", n=2)
                counts["functions"] += 1
                if d.sites or d.unaligned:
                    left |= report(d, key, known, out, counts)
                else:
                    counts["equal"] += 1
    print("\n".join(out))
    print(", ".join(f"{n} {k}" for k, n in sorted(counts.items())) or "nothing compared")
    return 1 if left else 0


def main() -> int:
    parser = argparse.ArgumentParser(description="The decomp built with the late revision's NW_REV values against the "
                                                 "PAL v2.01 and USA v1.01 code, through port/revmap.")
    sub = parser.add_subparsers(dest="cmd", required=True)
    for name in ("functions", "check"):
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
    p = sub.add_parser("compile", help=argparse.SUPPRESS)
    p.add_argument("--decomp", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("units", nargs="+")
    args = parser.parse_args()
    return {"functions": cmd_functions, "check": cmd_check, "compile": cmd_compile}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
