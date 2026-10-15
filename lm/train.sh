#!/bin/sh
# 重現字元 5-gram 語言模型。需要：KenLM（lmplz、build_binary 在 PATH 上）、pip install opencc kenlm
# 語料：Wiki40B zh-tw（CC BY-SA 3.0）、PTT 八卦版問答（Apache-2.0）。約需 15 GB 記憶體、30 分鐘。
set -eu
cd "$(dirname "$0")/.."
C=${CORPUS_DIR:-corpus}
mkdir -p "$C/wiki40b"
B=https://storage.googleapis.com/tfds-data/datasets/wiki40b/Wiki40B.zh-tw/1.0.0
for f in wiki40b-validation.tfrecord-00000-of-00001 wiki40b-test.tfrecord-00000-of-00001 \
         $(for i in 0 1 2 3 4 5 6 7; do echo wiki40b-train.tfrecord-0000$i-of-00008; done); do
  [ -f "$C/wiki40b/$f" ] || curl -sSf -o "$C/wiki40b/$f" "$B/$f"
done
[ -d "$C/ptt" ] || git clone -q --depth 1 https://github.com/zake7749/Gossiping-Chinese-Corpus "$C/ptt"

# Wiki40B 的 test 分割與 PTT 每 50 行取 1 行保留給開發集，不參與訓練
python3 lm/extract_wiki40b.py "$C"/wiki40b/wiki40b-test.* > "$C/wiki_heldout.txt"
python3 lm/extract_wiki40b.py "$C"/wiki40b/wiki40b-train.* "$C"/wiki40b/wiki40b-validation.* > "$C/wiki.txt"
python3 lm/extract_ptt.py "$C/ptt/data/Gossiping-QA-Dataset-2_0.csv" > "$C/ptt.txt"
awk 'NR%50==0' "$C/ptt.txt" > "$C/ptt_heldout.txt"
awk 'NR%50!=0' "$C/ptt.txt" > "$C/ptt_train.txt"

# PTT 重複 6 次，提高口語的比重；輸入法內附的模型即此設定（mac/McBopomofo/Source/Data/context-lm）
(cat "$C/wiki.txt"; for i in 1 2 3 4 5 6; do cat "$C/ptt_train.txt"; done) |
  lmplz -o 5 --prune 0 2 4 6 8 -S 50% -T "$C/" --discount_fallback > "$C/char5.arpa"
build_binary -q 8 -b 8 -a 22 trie "$C/char5.arpa" "$C/char5.bin"
rm "$C/char5.arpa"
python3 lm/make_dev.py "$C/wiki_heldout.txt" "$C/ptt_heldout.txt"
