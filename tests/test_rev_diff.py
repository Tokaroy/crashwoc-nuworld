import re
import sys
import unittest
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
import ntsc_diff as nd  # noqa: E402
import rev_diff as tool  # noqa: E402

LI_A1_42 = 0x24050042
LI_A1_43 = 0x24050043
LI_V0_9 = 0x24020009
LI_V0_3 = 0x24020003
JAL = 0x0C000000
NOP = 0


def code(words) -> nd.Code:
    return nd.tokenize(nd.Code("f", "game/x", 0, list(words)), {})


class Known(nd.Known):
    def __init__(self, rules):
        self.rules = [(re.compile(p), r) for p, r in rules]
        self.rate, self.not_rate = [], []


class LineTest(unittest.TestCase):
    def test_line_numbers_before_prolog_calls_are_masked(self):
        a = code([LI_A1_42, JAL, NOP, LI_V0_9])
        b = code([LI_A1_43, JAL, NOP, LI_V0_9])
        lines = tool.line_loads(a, {4: "NuErrorProlog"})
        self.assertEqual(lines, {0})
        self.assertEqual(tool.diff(a, b, lines).sites, [])
        self.assertEqual(len(tool.diff(a, b, tool.line_loads(a, {4: "GameSfx"})).sites), 1)


class ReportTest(unittest.TestCase):
    def test_known_and_left(self):
        a = code([0x8C820168, LI_V0_9])
        b = code([0x8C820160, LI_V0_3])
        d = tool.diff(a, b, set())
        d.unit, d.name = "game/x", "f"
        counts, out = Counter(), []
        self.assertTrue(tool.report(d, "usa101", Known([]), out, counts))
        self.assertEqual(out, ["game/x f@usa101:", "  +0x0000 off 360 -> 352  review",
                               "  +0x0004 imm 9 -> 3  review"])
        counts, out = Counter(), []
        rules = [(r"^func:game/x/f@\w+\+0x4 imm 9 -> 3$", "a menu id")]
        self.assertTrue(tool.report(d, "usa101", Known(rules), out, counts))
        self.assertEqual(counts["review known"], 1)
        self.assertEqual(out, ["game/x f@usa101:", "  +0x0000 off 360 -> 352  review"])
        counts, out = Counter(), []
        self.assertTrue(tool.report(d, "pal201", Known(rules), out, counts))
        self.assertEqual(out, ["game/x f@pal201:", "  +0x0000 off 360 -> 352  review"])

    def test_sound_numbers(self):
        a = code([0x240500D0])
        b = code([0x240500B7])
        d = tool.diff(a, b, set())
        d.unit, d.name = "game/x", "f"
        counts, out = Counter(), []
        self.assertFalse(tool.report(d, "usa101", Known([]), out, counts, {(208, 183)}))
        self.assertEqual((counts["sound"], out), (1, []))
        counts, out = Counter(), []
        self.assertTrue(tool.report(d, "usa101", Known([]), out, counts, {(208, 188)}))
        self.assertEqual(out, ["game/x f@usa101:", "  +0x0000 imm 208 -> 183  review"])


class SoundTest(unittest.TestCase):
    def test_pairs_by_name(self):
        def entry(name, number):
            e = bytearray(nd.SOUND)
            e[:len(name)] = name
            e[0x22:0x24] = number.to_bytes(2, "little")
            return bytes(e)

        pal = {0x1000: entry(b"jump", 0) + entry(b"aku fre", 1) + entry(b"land", 2),
               0x2000: entry(b"aku", 0) + entry(b"aku", 0) + entry(b"boom", 0)}
        us = {0x5000: entry(b"jump", 0) + entry(b"land", 1), 0x6000: entry(b"aku", 0) + entry(b"boom", 0)}

        class Ref:
            objects = [(0x1000, 0x1000 + 3 * nd.SOUND, "SfxTabGLOBAL"), (0x2000, 0x2000 + 3 * nd.SOUND, "SfxTabX"),
                       (0x3000, 0x3010, "Other")]

            def read(self, addr, n):
                return pal[addr][:n] if addr in pal else None

        class Tgt:
            data_ref = {0x1000: (0x5000, "SfxTabGLOBAL"), 0x2000: (0x6000, "SfxTabX"),
                        0x3000: (0x6000 + 2 * nd.SOUND, "Other")}

            def read(self, addr, n):
                return us[addr][:n] if addr in us else None

        self.assertEqual(tool.sound_ids(Ref(), Tgt()), {(2, 1), (3, 2), (4, 2), (5, 3)})


if __name__ == "__main__":
    unittest.main()
