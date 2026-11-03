# word-lm

詞層級 3-gram 語言模型（KenLM 二進位格式），與 `context-lm`（字元模型）一起供「看整句選字」使用。
選字時每個詞（詞庫中的一個詞條）以整個詞為單位評分，補足字元模型看不出的詞與詞搭配，例如「支／影片」「比重／多」。
為了不超過 Git 平台的單檔大小限制，模型切成多個 `part-*` 檔，建置時由 `Source/Data/Makefile` 合併成 `word-lm.bin`。

- SHA-256（合併後）：`e70b0d25be4f07e757844fef47f69f7cf81750ec05d8eaaa69d09d50ac5c0327`
- 大小：101348174 位元組
- 斷詞：以小麥注音詞庫（`data.txt`）的詞頻做最大機率斷詞（`lm/segment.cpp`）
- 參數：`lmplz -o 3 --prune 0 1 2`（PTT 語料重複 6 次），`build_binary -q 8 -b 8 -a 22 trie`
- 重現方式：見專案根目錄的 `lm/train_words.sh`

## 訓練資料與授權

- Wiki40B zh-tw（Google，取自維基百科），CC BY-SA 3.0
- PTT 八卦版問答語料（zake7749/Gossiping-Chinese-Corpus），Apache-2.0

本模型由上述資料衍生，依 CC BY-SA 3.0 授權釋出：
https://creativecommons.org/licenses/by-sa/3.0/
