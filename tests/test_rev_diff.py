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
    def test_layout_known_and_left(self):
        a = code([0x8C820168, LI_V0_9])
        b = code([0x8C820160, LI_V0_3])
        d = tool.diff(a, b, set())
        d.unit, d.name = "game/x", "f"
        counts, out = Counter(), []
        self.assertTrue(tool.report(d, "usa101", Known([]), out, counts))
        self.assertEqual(counts["layout"], 1)
        self.assertEqual(out, ["game/x f@usa101:", "  +0x0004 imm 9 -> 3  review"])
        counts, out = Counter(), []
        rules = [(r"^func:game/x/f@\w+\+0x4 imm 9 -> 3$", "a menu id")]
        self.assertFalse(tool.report(d, "usa101", Known(rules), out, counts))
        self.assertEqual(counts["review known"], 1)
        counts, out = Counter(), []
        self.assertTrue(tool.report(d, "pal201", Known(rules), out, counts))
        self.assertEqual(out, ["game/x f@pal201:", "  +0x0000 off 360 -> 352  review"])


if __name__ == "__main__":
    unittest.main()
