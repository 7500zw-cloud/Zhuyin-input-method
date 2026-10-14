#!/usr/bin/env python3
"""看整句的注音轉換器（原型）。

1. 候選：每一段連續音節，從詞庫找出所有讀音相符的詞與單字，組成詞格（lattice）。
2. 評分：用字元 5-gram 語言模型為整句打分，加上詞庫頻率與斷詞數量兩項輔助分數。
3. 搜尋：由左到右的 beam search；語言模型狀態相同的路徑只保留最好的一條。

    python3 lm/decoder.py --lm corpus/char5.bin < data/testset.tsv > results/lm.tsv
"""

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

import kenlm

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from testset import load_chewing  # noqa: E402

MAX_PHRASE = 8


class Lexicon:
    """讀音 → [(詞, log 頻率)]"""

    def __init__(self):
        chars, phrases = load_chewing()
        table = {}
        for text, readings in phrases.items():
            for reading, freq in readings:
                table.setdefault(reading, {})
                table[reading][text] = max(table[reading].get(text, 0), freq)
        for ch, readings in chars.items():
            for r in readings:
                table.setdefault((r,), {}).setdefault(ch, 0)
        self.table = {
            reading: [(text, math.log10(freq + 1)) for text, freq in words.items()]
            for reading, words in table.items()
        }

    def lookup(self, syllables):
        return self.table.get(tuple(syllables), [])


@dataclass
class Params:
    beam: int = 30
    w_freq: float = 0.1   # 每個詞加上 w_freq × log10(詞庫頻率 + 1)
    w_seg: float = -1.0   # 每多切一段的加分（負值＝偏好長詞）


@dataclass
class Hyp:
    score: float
    text: str
    state: kenlm.State


class Decoder:
    def __init__(self, lm_path, lexicon=None, params=None):
        self.lm = kenlm.Model(str(lm_path))
        self.lex = lexicon or Lexicon()
        self.p = params or Params()

    def _extend(self, hyp, word, prior):
        state, score = hyp.state, 0.0
        for ch in word:
            out = kenlm.State()
            score += self.lm.BaseScore(state, ch, out)
            state = out
        return Hyp(hyp.score + score + self.p.w_seg + self.p.w_freq * prior, hyp.text + word, state)

    def decode(self, syllables, nbest=1):
        n = len(syllables)
        start = kenlm.State()
        self.lm.BeginSentenceWrite(start)
        chart = [[] for _ in range(n + 1)]
        chart[0] = [Hyp(0.0, "", start)]
        for i in range(n):
            if not chart[i]:
                continue
            for length in range(1, min(MAX_PHRASE, n - i) + 1):
                cands = self.lex.lookup(syllables[i:i + length])
                for hyp in chart[i]:
                    for word, prior in cands:
                        chart[i + length].append(self._extend(hyp, word, prior))
            for j in range(i + 1, n + 1):
                chart[j] = self._prune(chart[j])
        finals = []
        for hyp in chart[n]:
            end = kenlm.State()
            finals.append(Hyp(hyp.score + self.lm.BaseScore(hyp.state, "</s>", end), hyp.text, end))
        finals.sort(key=lambda h: h.score, reverse=True)
        return [h.text for h in finals[:nbest]]

    def _prune(self, hyps):
        # 語言模型狀態相同（最後 4 個字相同）的路徑，之後的分數都一樣，只留最好的
        best = {}
        for h in hyps:
            key = h.text[-4:]
            if key not in best or h.score > best[key].score:
                best[key] = h
        return sorted(best.values(), key=lambda h: h.score, reverse=True)[:self.p.beam]


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--lm", required=True, help="KenLM 二進位模型")
    ap.add_argument("--beam", type=int, default=Params.beam)
    ap.add_argument("--w-freq", type=float, default=Params.w_freq)
    ap.add_argument("--w-seg", type=float, default=Params.w_seg)
    args = ap.parse_args()

    dec = Decoder(args.lm, params=Params(args.beam, args.w_freq, args.w_seg))
    for line in sys.stdin.read().splitlines()[1:]:
        cols = line.split("\t")
        out = dec.decode(cols[3].split())
        print(f"{cols[0]}\t{out[0] if out else ''}")


if __name__ == "__main__":
    main()
