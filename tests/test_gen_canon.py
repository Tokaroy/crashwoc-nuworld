import importlib
import re
import struct
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))
gen_canon = importlib.import_module("gen_canon")


def fixture() -> tuple[bytes, bytes]:
    text = (REPO / "tests" / "c" / "rnc2_fixture.inc").read_text()
    body = text[text.index("{") + 1:text.index("}")]
    packed = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-f]{2})", body))
    payload = (b"CrashWOC NuWorld RNC2 fixture. " * 50)[:1200] + bytes((i * 7 + (i >> 3)) & 0xFF for i in range(400))
    return packed, payload


def chunk(tag: bytes, body: bytes) -> bytes:
    return tag + struct.pack("<i", 8 + len(body)) + body


def obj(lo: float, hi: float) -> bytes:
    geom = struct.pack("<iii", 0, 0, 0) + struct.pack("<i", 0)
    sub = struct.pack("<ii3fiii", 0, 1, 0, 0, 0, 0, 0, 0) + geom + struct.pack("<6fii", lo, lo, lo, hi, hi, hi, 0, 0)
    return struct.pack("<iiii", 1, 0, 0, 0) + sub


def inst(objid: int, x: float, anim: int = 0) -> bytes:
    mtx = (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, x, x, 1)
    return struct.pack("<16f", *mtx) + struct.pack("<iiihbb", objid, 5, anim, 0, 0, 0)


def scene(instances: list[bytes], specials: list[tuple[int, int]], splines: list[tuple[int, list[float]]],
          names: bytes, anims: int = 0, low: float = 0.0) -> bytes:
    objs = [obj(low, low + 1.0), obj(0.0, 2.0)]
    body = chunk(b"NTBL", struct.pack("<i", len(names)) + names)
    body += chunk(b"MS00", struct.pack("<ii", 1, 0) + b"\0" * 0x1A4 + struct.pack("<i", -1) + b"\0" * 0x28)
    body += chunk(b"OBJ0", struct.pack("<ii", len(objs), 0) + b"".join(objs))
    body += chunk(b"INST", struct.pack("<ii", len(instances), 0) + b"".join(instances))
    body += chunk(b"IABL", struct.pack("<ii", anims, 0) + b"\x11" * (0x60 * anims))
    body += chunk(b"SPEC", struct.pack("<ii", len(specials), 0) + b"".join(
        struct.pack("<16f", *([0.0] * 16)) + struct.pack("<iiii", i, n, 0, 0) for i, n in specials))
    sst = b"".join(struct.pack("<HHi", len(p) // 3, 12, n) + struct.pack(f"<{len(p)}f", *p) for n, p in splines)
    body += chunk(b"SST0", struct.pack("<ii", len(splines), len(sst)) + sst)
    return b"NU20" + struct.pack("<iii", -(16 + len(body)), 6, 0) + body


def ter(platforms: list[tuple[int, bytes]]) -> bytes:
    data = b"".join(d for _i, d in platforms)
    d = (4 + len(data)) // 2
    entries = b"".join(struct.pack("<i3fhh4h3fIhhi", len(b) // 2, 0, 0, 0, 1, i, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
                       for i, b in platforms)
    return struct.pack("<i", d) + data + struct.pack("<i", len(platforms)) + entries


class GenCanonTest(unittest.TestCase):
    def test_unpack(self):
        packed, payload = fixture()
        self.assertEqual(gen_canon.unpack(packed), payload)
        self.assertEqual(gen_canon.unpack(b"NU20 plain"), b"NU20 plain")
        with self.assertRaises(ValueError):
            gen_canon.unpack(packed[:4] + struct.pack(">I", 5) + packed[8:])

    def test_delta(self):
        src = bytes(range(256)) * 40
        cases = [src, src[:300] + b"inserted words" + src[300:], src[:100] + src[164:], src[:500] + b"\xff" * 4 +
                 src[504:], src[5000:] + src[:5000], b"", b"tiny"]
        for dst in cases:
            segs = gen_canon.delta(src, dst)
            words = []
            for _o, _n, lit in segs:
                words += [w for (w,) in struct.iter_unpack("<I", lit + b"\0" * (-len(lit) % 4))]
            self.assertEqual(gen_canon.rebuild(src, [(o, n, len(lit)) for o, n, lit in segs], words), dst)
        self.assertLess(sum(len(lit) for _o, _n, lit in gen_canon.delta(src, cases[1])), 32)
        self.assertLessEqual(len(gen_canon.delta(src, cases[4])), 3)

    def test_near(self):
        a = struct.pack("<3f", 1.0, -5.68, 0.0)
        self.assertTrue(gen_canon.near(a, struct.pack("<3f", 1.0, -5.6799998, -1.2e-7)))
        self.assertFalse(gen_canon.near(a, struct.pack("<3f", 1.0, -5.5, 0.0)))
        self.assertFalse(gen_canon.near(struct.pack("<I", 1), struct.pack("<I", 2)))

    def test_scene_reorder(self):
        names = b"plat\0weecam_mid_00\0"
        old = scene([inst(0, 0.0), inst(1, 10.0), inst(0, 20.0)], [(1, 0)], [(5, [0.0] * 6)], names)
        new = scene([inst(0, 20.0), inst(0, 0.0), inst(1, 10.0)], [(2, 0)], [(5, [1.0] * 6)], names)
        model = b"\x01\x02" * 8
        patched, notes = gen_canon.scene_patch(old, new, ter([(2, model)]), ter([(1, model)]))
        s, o, t = gen_canon.Scene(patched), gen_canon.Scene(old), gen_canon.Scene(new)
        self.assertEqual(s.instances[2], o.instances[1])
        self.assertEqual(sorted(s.instances), sorted(o.instances))
        self.assertEqual(s.specials, [("plat", 2)])
        self.assertEqual(s.splines, t.splines)
        self.assertEqual(s.data("OBJ0"), o.data("OBJ0"))
        self.assertTrue(any("moved" in n for n in notes))
        same, _notes = gen_canon.scene_patch(old, old, ter([(1, model)]), ter([(1, model)]))
        self.assertEqual(same, old)

    def test_scene_refused(self):
        names = b"plat\0weecam_mid_00\0"
        old = scene([inst(0, 0.0)], [(0, 0)], [(5, [0.0] * 3)], names)
        new = scene([inst(0, 0.0), inst(1, 50.0, anim=7)], [(0, 0)], [(5, [0.0] * 3)], names, anims=1)
        with self.assertRaisesRegex(gen_canon.Refused, "new animated instance 1"):
            gen_canon.scene_patch(old, new, ter([(1, b"\0" * 4)]), None)
        moved = scene([inst(0, 0.5)], [(0, 0)], [(5, [0.0] * 3)], names, low=-0.5)
        with self.assertRaisesRegex(gen_canon.Refused, "moved with its frame"):
            gen_canon.scene_patch(old, moved, ter([(0, b"\x05" * 4)]), ter([(0, b"\x06" * 4)]))

    def test_plan(self):
        packed, payload = fixture()
        dst = gen_canon.Unpacked({
            "LEVELS/A/X/X.CRT": b"\x04\0\0\0crates new",
            "LEVELS/A/X/X.CPT": b"\r\n\0\0" + b"\0" * 76,
            "LEVELS/A/X/X.AI": b"same",
            "LEVELS/A/X/X.VIS": b"\xff\xff\xff\xff vis",
            "LEVELS/A/X/FIRE.GSC": b"NU20" + b"\0" * 12,
            "LEVELS/A/X/X.PTL": payload,
        })
        src = {
            "LEVELS/A/X/X.CRT": b"\x04\0\0\0crates old",
            "LEVELS/A/X/X.CPT": b"\r\0\0\0\0\0\0\0",
            "LEVELS/A/X/X.AI": b"same",
            "LEVELS/A/X/X.PTL": packed,
        }
        out = {k: (raw, result, sc) for k, raw, result, sc in gen_canon.plan(src, dst, [])}
        self.assertEqual(sorted(out), ["LEVELS/A/X/FIRE.GSC", "LEVELS/A/X/X.CRT", "LEVELS/A/X/X.VIS"])
        self.assertEqual(out["LEVELS/A/X/X.CRT"], (src["LEVELS/A/X/X.CRT"], b"\x04\0\0\0crates new", False))
        self.assertEqual(out["LEVELS/A/X/FIRE.GSC"][0], None)

    def test_generate(self):
        with tempfile.TemporaryDirectory() as tmp:
            roots = {}
            for disc, data in (("pal103", b"old crates"), ("usa100", b"old crates"), ("pal201", b"new crates"),
                               ("us101", b"new crates")):
                root = Path(tmp) / disc
                (root / "LEVELS" / "A" / "X").mkdir(parents=True)
                (root / "LEVELS" / "A" / "X" / "X.CRT").write_bytes(data)
                (root / "LEVELS" / "B" / "Y").mkdir(parents=True)
                (root / "LEVELS" / "B" / "Y" / "Y.CRT").write_bytes(data + disc.encode())
                roots[disc] = root
            (roots["us101"] / "LEVELS" / "A" / "X" / "X.VIS").write_bytes(b"\xff\xff\xff\xffvis")
            text = gen_canon.generate(roots)
            self.assertEqual(text, gen_canon.generate(roots))
            rows = re.findall(r'\{ &(\w+), "([^"]+)", (NULL|"\w+"), "(\w+)", (\d+)u, (\d+)u, (\d+)u, (\d+)u', text)
            self.assertEqual([(r[0], r[1]) for r in rows], [
                ("nw_disc_manifest", "LEVELS/A/X/X.CRT"), ("nw_disc_manifest", "LEVELS/A/X/X.VIS"),
                ("nw_disc_manifest_usa100", "LEVELS/A/X/X.CRT"), ("nw_disc_manifest_usa100", "LEVELS/A/X/X.VIS"),
                ("nw_disc_manifest_pal201", "LEVELS/A/X/X.VIS")])
            self.assertEqual(rows[0][4:], rows[2][4:])
            self.assertEqual(rows[1][2], "NULL")
            self.assertNotIn("LEVELS/B", text)
            self.assertIn("\r\n", text)


if __name__ == "__main__":
    unittest.main()
