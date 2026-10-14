import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "lm"))
LMPLZ = Path(os.environ.get("LMPLZ") or shutil.which("lmplz") or "/home/user/kenlm/build/bin/lmplz")

try:
    import kenlm  # noqa: F401
    from decoder import Decoder, Lexicon, Params
except ImportError:
    kenlm = None

CORPUS = ["好 用 的 定 義 是 什 麼", "這 個 詞 的 定 義 是 什 麼", "他 的 意 識 很 清 楚",
          "我 們 去 訂 票", "人 民 有 選 擇 的 權 利", "總 統 的 權 力 很 大"] * 5


class FakeLexicon(Lexicon):
    def __init__(self):
        self.table = {
            ("ㄉㄧㄥˋ", "ㄧˋ"): [("定義", 3.0)], ("ㄧˋ", "ㄕˋ"): [("意識", 3.3)],
            ("ㄉㄧㄥˋ",): [("定", 0.0), ("訂", 0.0)], ("ㄧˋ",): [("義", 0.0), ("意", 0.0)],
            ("ㄕˋ",): [("是", 0.0), ("識", 0.0)], ("ㄏㄠˇ",): [("好", 0.0)],
            ("ㄩㄥˋ",): [("用", 0.0)], ("ㄉㄜ˙",): [("的", 0.0)],
            ("ㄕㄣˊ", "ㄇㄜ˙"): [("什麼", 4.0)],
        }


@unittest.skipUnless(kenlm and LMPLZ.exists(), "需要 kenlm 與 lmplz")
class DecoderTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        arpa = Path(cls.tmp.name) / "tiny.arpa"
        with open(arpa, "w") as out:
            subprocess.run([LMPLZ, "-o", "3", "--discount_fallback", "-S", "10%"],
                           input="\n".join(CORPUS) + "\n", text=True, stdout=out,
                           stderr=subprocess.DEVNULL, check=True)
        cls.dec = Decoder(arpa, FakeLexicon(), Params(beam=10))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_context_beats_frequency(self):
        # 「意識」詞頻較高，但整句來看「定義是」比較通順
        syl = "ㄏㄠˇ ㄩㄥˋ ㄉㄜ˙ ㄉㄧㄥˋ ㄧˋ ㄕˋ ㄕㄣˊ ㄇㄜ˙".split()
        self.assertEqual(self.dec.decode(syl)[0], "好用的定義是什麼")

    def test_nbest_distinct(self):
        out = self.dec.decode("ㄉㄧㄥˋ ㄧˋ".split(), nbest=5)
        self.assertEqual(len(out), len(set(out)))
        self.assertIn("定義", out)

    def test_unknown_syllable(self):
        self.assertEqual(self.dec.decode(["ㄅㄧㄤ"]), [])
