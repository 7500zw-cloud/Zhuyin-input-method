#!/usr/bin/env python3
"""為 data/testset.tsv 中 zhuyin 欄空白的句子自動標注台灣讀音。

做法：
1. 用新酷音詞庫把正確句子斷詞（詞越長越好，同長度取頻率高者）。
2. 每個詞從詞庫讀音中選一個；有多個讀音時，以跟 pypinyin 一致的字數最多者優先。
   讀音一律採用詞庫有收錄的寫法（例如「不要」詞庫只收 ㄅㄨˊ ㄧㄠˋ），
   避免把「讀音習慣不同」誤算成「選字錯誤」。
3. 新酷音與 pypinyin 不一致、或單字有多音無法判斷的地方，列出來讓人工檢查。

已有注音的列不會被覆蓋（人工修正會保留），除非加上 --force。
"""

import argparse
import math
import sys

from pypinyin import Style, pinyin

from testset import is_syllable, load_chewing, load_testset, save_testset


def pypinyin_readings(text):
    out = []
    for item in pinyin(text, style=Style.BOPOMOFO, errors=lambda x: [None] * len(x)):
        out.append(item[0] if item else None)
    return out


def segment(text, chars, phrases, max_len=8):
    """回傳 [(start, end)]，段數最少、同段數時總頻率最高。"""
    n = len(text)
    best = [(math.inf, 0.0, None)] * (n + 1)  # (段數, -log頻率和, 前一個切點)
    best[0] = (0, 0.0, None)
    for i in range(n):
        if best[i][0] == math.inf:
            continue
        for j in range(i + 1, min(n, i + max_len) + 1):
            word = text[i:j]
            if j - i == 1:
                if word not in chars and word not in phrases:
                    continue
                freq = max((f for _, f in phrases.get(word, [])), default=0)
            elif word in phrases:
                freq = max(f for _, f in phrases[word])
            else:
                continue
            cand = (best[i][0] + 1, best[i][1] - math.log(freq + 1), i)
            if cand[:2] < best[j][:2]:
                best[j] = cand
    if best[n][0] == math.inf:
        missing = [c for c in text if c not in chars and c not in phrases]
        raise ValueError(f"詞庫缺字：{''.join(missing)}")
    cuts, j = [], n
    while j:
        i = best[j][2]
        cuts.append((i, j))
        j = i
    return cuts[::-1]


def annotate(text, chars, phrases):
    ref = pypinyin_readings(text)
    result, flags = [], []
    for i, j in segment(text, chars, phrases):
        word = text[i:j]
        candidates = list(phrases.get(word, []))
        if j - i == 1:
            candidates += [((r,), 0) for r in sorted(chars.get(word, ()))]
        def score(c):
            agree = sum(r == ref[i + k] for k, r in enumerate(c[0]))
            return (agree, c[1])
        candidates.sort(key=score, reverse=True)
        reading = candidates[0][0]
        result.extend(reading)
        distinct = {c[0] for c in candidates}
        for k, r in enumerate(reading):
            if ref[i + k] is not None and r != ref[i + k]:
                flags.append(f"{word}[{word[k]}]：新酷音 {r}／pypinyin {ref[i + k]}")
            elif j - i == 1 and len(distinct) > 1 and r != ref[i + k]:
                flags.append(f"{word}：多音字，候選 {'、'.join(c[0][0] for c in candidates)}")
    return result, flags


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--force", action="store_true", help="重新標注所有句子")
    args = ap.parse_args()

    chars, phrases = load_chewing()
    rows = load_testset()
    n_done = n_flag = 0
    for row in rows:
        if row["zhuyin"] and not args.force:
            continue
        try:
            syllables, flags = annotate(row["text"], chars, phrases)
        except ValueError as e:
            print(f"{row['id']} {row['text']}：{e}", file=sys.stderr)
            continue
        assert all(is_syllable(s) for s in syllables), syllables
        row["zhuyin"] = " ".join(syllables)
        n_done += 1
        if flags:
            n_flag += 1
            print(f"{row['id']} {row['text']}\n    {row['zhuyin']}")
            for f in flags:
                print(f"    ⚠ {f}")
    save_testset(rows)
    print(f"\n標注 {n_done} 句，其中 {n_flag} 句需要人工確認。", file=sys.stderr)


if __name__ == "__main__":
    main()
