import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from evaluate import edit_distance, score_row, summarize  # noqa: E402
from testset import is_syllable, load_testset, normalize  # noqa: E402
from validate import validate  # noqa: E402

ROW = {"id": "seg-001", "category": "seg", "text": "好用的定義是什麼",
       "zhuyin": "ㄏㄠˇ ㄩㄥˋ ㄉㄜ˙ ㄉㄧㄥˋ ㄧˋ ㄕˋ ㄕㄣˊ ㄇㄜ˙",
       "focus": "定義是", "trap": "訂意識", "note": ""}


class SyllableTest(unittest.TestCase):
    def test_valid(self):
        for s in ["ㄏㄠˇ", "ㄧ", "ㄕ", "ㄇㄜ˙", "ㄓㄨㄤ", "ㄩㄝˋ", "ㄦˊ"]:
            self.assertTrue(is_syllable(s), s)

    def test_invalid(self):
        for s in ["", "ˇ", "ㄏㄏ", "ㄠㄏ", "hao3", "ㄏㄠˇˇ"]:
            self.assertFalse(is_syllable(s), s)


class ScoreTest(unittest.TestCase):
    def test_exact(self):
        s = score_row(ROW, "好用的定義是什麼")
        self.assertTrue(s["exact"] and s["focus"])
        self.assertFalse(s["trap"])
        self.assertEqual(s["char_errors"], 0)

    def test_trap(self):
        s = score_row(ROW, "好用的訂意識什麼")
        self.assertFalse(s["exact"] or s["focus"])
        self.assertTrue(s["trap"])
        self.assertEqual(s["char_errors"], 3)

    def test_error_outside_focus(self):
        s = score_row(ROW, "好用得定義是什麼")
        self.assertFalse(s["exact"])
        self.assertTrue(s["focus"])

    def test_variants_are_equal(self):
        self.assertEqual(normalize("臺灣週末"), normalize("台灣周末"))

    def test_edit_distance(self):
        self.assertEqual(edit_distance("abc", "abc"), 0)
        self.assertEqual(edit_distance("abc", "ab"), 1)
        self.assertEqual(edit_distance("", "ab"), 2)

    def test_summarize(self):
        total = summarize([score_row(ROW, ROW["text"]), score_row(ROW, "好用的訂意識什麼")])
        self.assertEqual(total["exact"], 0.5)
        self.assertEqual(total["traps"], 1)


class ValidateTest(unittest.TestCase):
    def test_testset_is_valid(self):
        self.assertEqual(validate(load_testset()), [])

    def test_catches_errors(self):
        bad = dict(ROW, zhuyin="ㄏㄠˇ ㄩㄥˋ", focus="不存在")
        errors = validate([bad, dict(ROW)])
        self.assertTrue(any("音節" in e for e in errors))
        self.assertTrue(any("重複" in e for e in errors))

    def test_reading_check(self):
        chars = {c: {s} for c, s in zip(ROW["text"], ROW["zhuyin"].split())}
        self.assertEqual(validate([ROW], chars), [])
        chars["義"] = {"ㄧˊ"}
        self.assertEqual(len(validate([ROW], chars)), 1)


if __name__ == "__main__":
    unittest.main()
