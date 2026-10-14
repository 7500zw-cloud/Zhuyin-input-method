# context-lm

字元 5-gram 語言模型（KenLM 二進位格式），供「看整句選字」（ContextualWalk）使用。
為了不超過 Git 平台的單檔大小限制，模型切成多個 `part-*` 檔，建置時由 `Source/Data/Makefile` 合併成 `context-lm.bin`。

- SHA-256（合併後）：`08b3d3e595f4bc9cac6236db539930daed3408dd6befe742ee39db20758f0ab9`
- 大小：66,117,919 位元組
- 參數：`lmplz -o 5 --prune 0 2 4 6 8`，`build_binary -q 8 -b 8 -a 22 trie`
- 重現方式：見專案根目錄的 `lm/train.sh`

## 訓練資料與授權

- Wiki40B zh-tw（Google，取自維基百科），CC BY-SA 3.0
- PTT 八卦版問答語料（zake7749/Gossiping-Chinese-Corpus），Apache-2.0

本模型由上述資料衍生，依 CC BY-SA 3.0 授權釋出：
https://creativecommons.org/licenses/by-sa/3.0/
