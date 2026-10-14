#!/usr/bin/env python3
"""把 PTT 八卦版問答語料（zake7749/Gossiping-Chinese-Corpus）轉成訓練文字，補足口語用法。

    python3 lm/extract_ptt.py corpus/ptt/data/Gossiping-QA-Dataset-2_0.csv > corpus/ptt.txt
"""

import csv
import sys

import opencc

from extract_wiki40b import tokenize


def main():
    to_tw = opencc.OpenCC("s2tw")
    csv.field_size_limit(10**7)
    with open(sys.argv[1], encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            for field in ("question", "answer"):
                text = row.get(field) or ""
                if text == "沒有資料":
                    continue
                for clause in tokenize(to_tw.convert(text)):
                    print(clause)


if __name__ == "__main__":
    main()
