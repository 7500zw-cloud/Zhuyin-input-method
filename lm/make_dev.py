#!/usr/bin/env python3
"""從未參與訓練的語料抽出開發集（data/dev.tsv），用來調整解碼參數。

測試集（data/testset.tsv）只用來報告最後成績，不拿來調參數。
開發集的注音由 tools/annotate.py 的方法自動標注，沒有人工檢查，難免有少數讀音錯誤；
它只用於比較不同參數的相對好壞。

來源：Wiki40B zh-tw 的 test 分割（CC BY-SA 3.0）與 PTT 八卦版語料每 50 行取 1 行（Apache-2.0）。

    python3 lm/make_dev.py corpus/wiki_heldout.txt corpus/ptt_heldout.txt
"""

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from annotate import annotate  # noqa: E402
from testset import ROOT, load_chewing, save_testset  # noqa: E402

PER_SOURCE = 300


def sample(path, chars, phrases, rng, n):
    with open(path, encoding="utf-8") as f:
        lines = [l.split() for l in f if "<x>" not in l]
    lines = [l for l in lines if 6 <= len(l) <= 16]
    rng.shuffle(lines)
    out = []
    for tokens in lines:
        text = "".join(tokens)
        try:
            syllables, _ = annotate(text, chars, phrases)
        except ValueError:
            continue
        out.append((text, syllables))
        if len(out) == n:
            break
    return out


def main():
    wiki, ptt = sys.argv[1:3]
    chars, phrases = load_chewing()
    rng = random.Random(20261014)
    rows = []
    for tag, path in (("wiki", wiki), ("ptt", ptt)):
        for k, (text, syl) in enumerate(sample(path, chars, phrases, rng, PER_SOURCE), 1):
            rows.append({"id": f"{tag}-{k:03d}", "category": tag, "text": text,
                         "zhuyin": " ".join(syl), "focus": text, "trap": "", "note": ""})
    save_testset(rows, ROOT / "data" / "dev.tsv")
    print(f"{len(rows)} 句", file=sys.stderr)


if __name__ == "__main__":
    main()
