#!/usr/bin/env python3
"""在開發集上以格點搜尋調整解碼參數（不使用測試集）。

    python3 lm/tune.py --lm corpus/char5.bin
"""

import argparse
import itertools
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from decoder import Decoder, Params  # noqa: E402
from evaluate import score_row, summarize  # noqa: E402
from testset import ROOT, load_testset  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lm", required=True)
    ap.add_argument("--dev", default=ROOT / "data" / "dev.tsv")
    args = ap.parse_args()

    rows = load_testset(args.dev)
    dec = Decoder(args.lm)
    results = []
    for w_freq, w_seg in itertools.product([0, 0.05, 0.1, 0.2, 0.4], [0, -0.25, -0.5, -1.0, -1.5]):
        dec.p = Params(beam=30, w_freq=w_freq, w_seg=w_seg)
        scores = [score_row(r, (dec.decode(r["zhuyin"].split()) or [""])[0]) for r in rows]
        s = summarize(scores)
        results.append((s["exact"], s["char_acc"], w_freq, w_seg))
        print(f"w_freq={w_freq:<5} w_seg={w_seg:<6} 整句 {s['exact']:.1%} 字 {s['char_acc']:.1%}", flush=True)
    best = max(results)
    print(f"最佳：w_freq={best[2]} w_seg={best[3]}（整句 {best[0]:.1%}）")


if __name__ == "__main__":
    main()
