#!/usr/bin/env python3
"""檢查 data/testset.tsv 格式是否正確。

基本檢查不需外部資料；若已執行 tools/fetch_chewing_data.sh，
會再確認每個字的讀音都存在於新酷音字庫中。
"""

import sys

from testset import CATEGORIES, CHEWING_DICT, is_syllable, load_chewing, load_testset


def validate(rows, chars=None):
    errors = []
    seen = set()
    for n, r in enumerate(rows, start=2):
        where = f"第 {n} 行 {r.get('id')}"
        if r["id"] in seen:
            errors.append(f"{where}：id 重複")
        seen.add(r["id"])
        if r["category"] not in CATEGORIES:
            errors.append(f"{where}：未知分類 {r['category']}")
        if not r["id"].startswith(r["category"] + "-"):
            errors.append(f"{where}：id 前綴與分類不符")
        text, syllables = r["text"], r["zhuyin"].split()
        if len(text) != len(syllables):
            errors.append(f"{where}：{len(text)} 個字但有 {len(syllables)} 個音節")
            continue
        for ch, s in zip(text, syllables):
            if not is_syllable(s):
                errors.append(f"{where}：「{ch}」的注音 {s} 格式錯誤")
            elif chars is not None and s not in chars.get(ch, ()):
                errors.append(f"{where}：新酷音字庫中「{ch}」沒有 {s} 這個讀音")
        if not r["focus"] or r["focus"] not in text:
            errors.append(f"{where}：focus「{r['focus']}」不在句子中")
        if r["trap"] and len(r["trap"]) > len(text):
            errors.append(f"{where}：trap 比句子還長")
    return errors


def main():
    rows = load_testset()
    chars = load_chewing()[0] if CHEWING_DICT.exists() else None
    errors = validate(rows, chars)
    for e in errors:
        print(e)
    note = "（含新酷音讀音比對）" if chars else "（未比對新酷音讀音）"
    print(f"{len(rows)} 句，{len(errors)} 個錯誤{note}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
