import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import revmap as tool  # noqa: E402

JR_RA, NOP = 0x03E00008, 0
BASE = 0x100000


def fn(seed: int, n: int = 10) -> list[int]:
    body = [0x27BDFFF0]
    for i in range(n):
        rs, rt, rd = (seed + i) % 31 + 1, (seed * 7 + i) % 31 + 1, (seed * 3 + i) % 31 + 1
        body.append(rs << 21 | rt << 16 | rd << 11 | 0x21)
    return body + [JR_RA, 0x27BD0010]


def image(parts: list[tuple[str, list[int]]], named: bool = True, objects=(), memory=()) -> tool.Image:
    words, funcs = [], []
    for name, body in parts:
        funcs.append(tool.Func(name, BASE + 4 * len(words), 4 * len(body)))
        words += body
    return tool.Image(BASE, words, 0, funcs if named else [], sorted(objects), list(memory))


def load(obj_hi: int, obj_lo: int) -> list[int]:
    return [0x27BDFFF0, 0x3C040000 | obj_hi, 0x8C820000 | obj_lo, 0x00431021, 0x00621821, 0x00832021,
            0x00A42821, 0x00C53021, 0x00E63821, JR_RA, 0x27BD0010]


class TokenTest(unittest.TestCase):
    def test_masks_addresses_keeps_shape(self):
        self.assertEqual(tool.token(0x3C041234), tool.token(0x3C045678))
        self.assertEqual(tool.token(0x0C040000), tool.token(0x0C041234))
        self.assertNotEqual(tool.token(0x3C041234), tool.token(0x3C051234))
        self.assertEqual(tool.token(0x45010003), tool.token(0x45010009))
        self.assertNotEqual(tool.token(0x00431021), tool.token(0x00431023))

    def test_lis(self):
        self.assertEqual(tool.lis([(0, 5), (1, 1), (2, 2), (3, 9), (4, 3)]), [(1, 1), (2, 2), (4, 3)])
        self.assertEqual(tool.lis([]), [])


class AlignTest(unittest.TestCase):
    def test_changed_moved_new(self):
        a, b, c, d, n = fn(1), fn(2), fn(3, 14), fn(4), fn(9, 12)
        b2 = list(b)
        b2[4] = 0x00E73821
        ref = image([("A", a), ("B", b), ("C", c), ("D", d)])
        tgt = image([("A", a), ("B", b2), ("N", n), ("D", d), ("C", c)], named=False)
        m = tool.align(ref, tgt)
        at = {p.func.name: p for p in m.placed}
        truth = {f.name: f.addr for f in image([("A", a), ("B", b2), ("N", n), ("D", d), ("C", c)]).funcs}
        self.assertEqual({k: p.addr for k, p in at.items()}, {k: truth[k] for k in "ABCD"})
        self.assertTrue(at["A"].same and at["C"].same and at["D"].same)
        self.assertFalse(at["B"].same)
        self.assertEqual(at["A"].size, 4 * len(a))
        self.assertEqual(m.removed, [])
        self.assertIn(truth["N"], [addr for addr, _ in m.new])

    def test_removed(self):
        ref = image([("A", fn(1)), ("B", fn(2)), ("C", fn(3))])
        tgt = image([("A", fn(1)), ("C", fn(3))], named=False)
        m = tool.align(ref, tgt)
        self.assertEqual([f.name for f in m.removed], ["B"])

    def test_data_from_code_and_neighbours(self):
        blob = bytes(range(0x40))
        ref = image([("A", fn(1)), ("L", load(0x60, 0x1000)), ("M", load(0x60, 0x1020))],
                    objects=[(0x601000, 16, "X"), (0x601010, 8, "Y"), (0x601020, 16, "Z")],
                    memory=[(0x601000, blob)])
        tgt = image([("A", fn(1)), ("L", load(0x60, 0x1200)), ("M", load(0x60, 0x1220))], named=False,
                    memory=[(0x601200, blob)])
        m = tool.align(ref, tgt)
        self.assertEqual({name: b for _, b, name, _, _ in m.data}, {"X": 0x601200, "Y": 0x601210, "Z": 0x601220})
        self.assertEqual(tool.lines(m)[-3:], ["data 00601000 00601200 X", "data 00601010 00601210 Y",
                                              "data 00601020 00601220 Z"])


class AddressesTest(unittest.TestCase):
    def test_names_and_translation(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "x.txt"
            path.write_text("func 00100000 00100010 20 A\ndata 00600000 00600100 X\ndata 00600040 00600150 Y\n")
            a = tool.Addresses("x", path)
            self.assertEqual(a.names, {"A": 0x100010, "X": 0x600100, "Y": 0x600150})
            self.assertEqual(a.translate(0x600008), 0x600108)
            self.assertEqual(a.translate(0x600044), 0x600154)
            with self.assertRaises(KeyError):
                a.translate(0x500000)


if __name__ == "__main__":
    unittest.main()
