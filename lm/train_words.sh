#!/bin/sh
# 重現詞層級 3-gram 語言模型（mac/McBopomofo/Source/Data/word-lm）。
# 先執行 lm/train.sh 產生 corpus/wiki.txt 與 corpus/ptt_train.txt，並以
# make -C mac/McBopomofo/Source/Data data.txt 產生詞庫。需要 KenLM 與 C++17 編譯器。
set -eu
cd "$(dirname "$0")/.."
C=${CORPUS_DIR:-corpus}
D=mac/McBopomofo/Source/Data/data.txt
mkdir -p "$C/word"
c++ -O2 -std=c++17 lm/segment.cpp -o "$C/word/segment"

# 用輸入法詞庫的詞頻斷詞，讓模型的「詞」與選字時的詞條一致
"$C/word/segment" "$D" < "$C/wiki.txt" > "$C/word/wiki.seg"
"$C/word/segment" "$D" < "$C/ptt_train.txt" > "$C/word/ptt_train.seg"

# PTT 重複 6 次，與字元模型相同
(cat "$C/word/wiki.seg"; for i in 1 2 3 4 5 6; do cat "$C/word/ptt_train.seg"; done) |
  lmplz -o 3 --prune 0 1 2 -S 50% -T "$C/word/" --discount_fallback > "$C/word/w3.arpa"
build_binary -q 8 -b 8 -a 22 trie "$C/word/w3.arpa" "$C/word/w3.bin"
rm "$C/word/w3.arpa"
