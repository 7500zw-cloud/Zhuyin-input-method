#!/usr/bin/env python3
"""用測試集為某個輸入法的轉換結果評分。

輸入：predictions.tsv，每行「id<TAB>輸入法輸出的句子」。
輸出：整句正確率、字正確率、重點詞正確率，以及各分類的成績與錯誤清單。

    python3 tools/evaluate.py results/chewing-0.13.1.tsv
"""

import argparse
import csv
import json
import sys

from testset import TESTSET, load_testset, normalize


def edit_distance(a, b):
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return prev[-1]


def score_row(row, output):
    gold, pred = normalize(row["text"]), normalize(output)
    focus = normalize(row["focus"])
    if len(pred) == len(gold):
        start = gold.index(focus)
        focus_ok = pred[start:start + len(focus)] == focus
    else:
        focus_ok = focus in pred
    trap = normalize(row["trap"])
    return {
        "exact": pred == gold,
        "char_errors": edit_distance(gold, pred),
        "chars": len(gold),
        "focus": focus_ok,
        "trap": bool(trap) and trap in pred and trap not in gold,
    }


def summarize(scores):
    n = len(scores)
    chars = sum(s["chars"] for s in scores)
    return {
        "sentences": n,
        "exact": sum(s["exact"] for s in scores) / n,
        "char_acc": 1 - sum(s["char_errors"] for s in scores) / chars,
        "focus": sum(s["focus"] for s in scores) / n,
        "traps": sum(s["trap"] for s in scores),
    }


def load_predictions(path):
    with open(path, encoding="utf-8", newline="") as f:
        return {r[0]: r[1] if len(r) > 1 else "" for r in csv.reader(f, delimiter="\t") if r}


def main():
    ap = argparse.ArgumentParser(description="為輸入法轉換結果評分")
    ap.add_argument("predictions")
    ap.add_argument("--testset", default=TESTSET, help="測試集路徑（預設 data/testset.tsv）")
    ap.add_argument("--json", help="另存完整結果為 JSON")
    ap.add_argument("--quiet", action="store_true", help="不列出錯誤句子")
    args = ap.parse_args()

    rows = load_testset(args.testset)
    preds = load_predictions(args.predictions)
    missing = [r["id"] for r in rows if r["id"] not in preds]
    if missing:
        print(f"缺少 {len(missing)} 句的結果：{', '.join(missing[:10])}", file=sys.stderr)
        return 1

    per_row = {r["id"]: score_row(r, preds[r["id"]]) for r in rows}
    total = summarize(list(per_row.values()))
    by_cat = {
        c: summarize([per_row[r["id"]] for r in rows if r["category"] == c])
        for c in dict.fromkeys(r["category"] for r in rows)
    }

    print(f"{'分類':<6}{'句數':>5}{'整句':>8}{'字':>8}{'重點詞':>8}{'掉陷阱':>6}")
    for c, s in [*by_cat.items(), ("全部", total)]:
        print(f"{c:<6}{s['sentences']:>6}{s['exact']:>9.1%}{s['char_acc']:>9.1%}"
              f"{s['focus']:>9.1%}{s['traps']:>7}")

    if not args.quiet:
        print("\n錯誤句子（✗ 表示重點詞也錯）：")
        for r in rows:
            s = per_row[r["id"]]
            if not s["exact"]:
                mark = "✓" if s["focus"] else "✗"
                print(f"  {mark} {r['id']:<9} 正確：{r['text']}\n"
                      f"    {'':<9} 輸出：{preds[r['id']]}")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump({"total": total, "by_category": by_cat,
                       "rows": {k: {**v, "output": preds[k]} for k, v in per_row.items()}},
                      f, ensure_ascii=False, indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
