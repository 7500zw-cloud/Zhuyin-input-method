#!/usr/bin/env python3
"""計算整句完全正確的句數。適用於含標點的段落檔（tools/evaluate.py 只接受測試集格式）。

    python3 tools/exact.py reference.tsv output.tsv
"""

import sys


def main():
    with open(sys.argv[1], encoding="utf-8") as f:
        ref = {l.split("\t")[0]: l.split("\t")[2] for l in f.read().splitlines()[1:]}
    with open(sys.argv[2], encoding="utf-8") as f:
        out = {}
        for l in f.read().splitlines():
            cols = l.split("\t") + [""]
            out[cols[0]] = cols[1]
    print(sum(ref[k] == out.get(k) for k in ref))


if __name__ == "__main__":
    main()
