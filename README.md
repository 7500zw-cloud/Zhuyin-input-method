# 注音輸入法選字測試集

目標是做出「會看上下文選字」的注音輸入法。動工之前，先用一套固定的測試句，量出現有輸入法實際錯在哪裡，之後每一次改進都用同一把尺檢驗。

起點是這個錯誤：想打「好用的**定義是**什麼」，結果出現「好用的**訂意識**什麼」。注音完全相同（ㄉㄧㄥˋ ㄧˋ ㄕˋ），錯在斷詞與選字沒有參考前後文。

## 目錄

```
data/testset.tsv          測試集（206 句）
tools/annotate.py         用新酷音詞庫自動標注台灣讀音，列出需人工確認處
tools/validate.py         檢查測試集格式與讀音
tools/evaluate.py         為輸入法輸出評分
tools/fetch_chewing_data.sh  下載新酷音詞庫（僅供標注與基準測試）
baselines/chewing/        新酷音 0.13.1 基準測試程式（Rust）
lm/                       看整句的解碼器原型、語料處理、語言模型訓練
mac/McBopomofo/           Mac 輸入法（以小麥注音為基礎，加入整句選字）
results/                  各輸入法的轉換結果與分數
tests/                    工具的單元測試
```

## 測試集格式

`data/testset.tsv`，以 Tab 分隔：

| 欄位 | 說明 |
|---|---|
| `id` | `分類-流水號`，例如 `seg-001` |
| `category` | 分類，見下表 |
| `text` | 正確句子 |
| `zhuyin` | 每個字一個音節，以空白分隔；一聲不標 |
| `focus` | 這句要測的重點詞，必須出現在 `text` 中 |
| `trap` | 已知的典型錯誤輸出（可空白） |
| `note` | 備註 |

| 分類 | 內容 | 句數 |
|---|---|---|
| `seg` | 斷詞歧義（定義是／訂意識、研究生命／研究生） | 30 |
| `homo` | 同音詞，要靠語境分辨（權利／權力、公式／公事／攻勢） | 51 |
| `func` | 虛詞（在／再、做／作、的／得、已／以） | 14 |
| `pron` | 代名詞（他／她／它／牠） | 6 |
| `long` | 一般語境 | 20 |
| `term` | 科技、新詞 | 20 |
| `name` | 地名、人名 | 15 |
| `chat` | 口語對話 | 20 |
| `num` | 數字、量詞 | 10 |
| `base` | 日常基本句，用來發現退步 | 20 |

### 讀音規則

- 以台灣讀音為準（研究 ㄐㄧㄡˋ、期 ㄑㄧˊ、垃圾 ㄌㄜˋ ㄙㄜˋ）。
- 只用新酷音詞庫有收錄的讀音。例如「不要」詞庫只收 ㄅㄨˊ ㄧㄠˋ，測試集就這樣標。這樣才不會把「讀音習慣不同」誤算成「選字錯誤」。
- 打字習慣的差異（不打變調、ㄣ／ㄥ 不分等）屬於「容錯」，日後另建測試集處理。

## 使用方式

```sh
pip install pypinyin opencc kenlm    # 標注、語料處理、解碼器需要
./tools/fetch_chewing_data.sh        # 下載新酷音詞庫
python3 tools/validate.py            # 檢查測試集
python3 -m unittest discover -s tests

# 新增句子：在 testset.tsv 加一列，zhuyin 留空，再執行
python3 tools/annotate.py            # 只填空白列，不會覆蓋人工修正

# 評分：準備「id<TAB>輸出句子」格式的檔案
python3 tools/evaluate.py results/chewing-0.13.1.tsv
```

### 重跑新酷音基準

```sh
cargo install chewing-cli --version =0.13.1
mkdir -p third_party/chewing-dict
for t in word tsi; do
  chewing-cli init-database --csv third_party/libchewing-data/dict/chewing/$t.csv third_party/chewing-dict/$t.dat
done
cargo build --release --manifest-path baselines/chewing/Cargo.toml
baselines/chewing/target/release/chewing-baseline third_party/chewing-dict \
  < data/testset.tsv > results/chewing-0.13.1.tsv
```

程式把每個音節轉成標準（大千）鍵盤按鍵，逐鍵送進新酷音，讀取組字區的整句結果。每句從空白狀態開始，不提交，也不學習。

## 指標

- **整句**：整句完全正確的比例。最嚴格，也最接近「打完不用改」的感受。
- **字**：1 − 編輯距離 ÷ 字數。
- **重點詞**：`focus` 那幾個字是否正確，衡量這句真正要測的歧義。
- **掉陷阱**：輸出中出現 `trap` 的句數。

`台／臺`、`裡／裏`、`周／週` 等異體字視為相同。

## 基準結果：新酷音 libchewing 0.13.1

詞庫 libchewing-data `c44e81a`（2026-03-22）。

| 分類 | 句數 | 整句 | 字 | 重點詞 |
|---|---:|---:|---:|---:|
| seg 斷詞 | 30 | 90.0% | 99.0% | 93.3% |
| homo 同音詞 | 51 | 54.9% | 92.4% | 68.6% |
| func 虛詞 | 14 | 78.6% | 97.0% | 85.7% |
| pron 代名詞 | 6 | 33.3% | 92.9% | 33.3% |
| long 一般語境 | 20 | 70.0% | 96.1% | 90.0% |
| term 科技新詞 | 20 | 75.0% | 97.3% | 95.0% |
| name 專有名詞 | 15 | 93.3% | 99.3% | 100.0% |
| chat 口語 | 20 | 80.0% | 96.1% | 90.0% |
| num 數字 | 10 | 50.0% | 92.0% | 70.0% |
| base 基本句 | 20 | 80.0% | 97.6% | 95.0% |
| **全部** | **206** | **71.8%** | **95.9%** | **84.0%** |

觀察：

- 「好用的定義是什麼」新酷音 0.13.1 **打得對**。原始錯誤可能來自其他輸入法或較舊的版本。
- 最弱的是**同音詞**：權利→全力、公事→公式、時限→實現、機智→機制、異議→意義。這類錯誤的特徵是前後文明明指向另一個詞，但它總是選頻率較高的那個。
- 單字層級的錯誤很多：在／再、背→被、店→電、道→到、這→蔗。這些是只看單一詞頻、不看前後字的典型結果。
- 代名詞（她、它、牠）幾乎都輸出「他」。

## 第 2 步：看整句的解碼器（原型）

`lm/decoder.py` 的做法：

1. **候選**：每一段連續音節，從詞庫找出讀音相符的所有詞與單字（目前沿用新酷音詞庫）。
2. **評分**：用字元 5-gram 語言模型為**整句**打分，再加上詞庫頻率與斷詞數兩項輔助分數。
3. **搜尋**：由左到右的 beam search（寬度 30）。

語言模型以 Wiki40B 繁體中文維基（約 2.5 億字）加上 PTT 八卦版問答（口語，重複 3 次提高比重）訓練，KenLM 二進位檔 381 MB。重現方式見 `lm/train.sh`。

參數只在**開發集**上調整：600 句，取自未參與訓練的維基與 PTT 文字（`lm/make_dev.py`），注音為自動標注。參數變化對結果影響不大（整句 78%～81%），表示效果主要來自語言模型本身。測試集只在參數定案後跑一次。

| 分類 | 新酷音 0.13.1 | 解碼器原型 |
|---|---:|---:|
| seg 斷詞 | 90.0% | 96.7% |
| homo 同音詞 | 54.9% | **90.2%** |
| func 虛詞 | 78.6% | 92.9% |
| pron 代名詞 | 33.3% | 33.3% |
| long 一般語境 | 70.0% | 95.0% |
| term 科技新詞 | 75.0% | 90.0% |
| name 專有名詞 | 93.3% | 100.0% |
| chat 口語 | 80.0% | 100.0% |
| num 數字 | 50.0% | 90.0% |
| base 基本句 | 80.0% | 95.0% |
| **全部（整句）** | **71.8%** | **92.2%** |
| 全部（重點詞） | 84.0% | 96.6% |
| 開發集（整句，600 句） | 47.2% | 81.2% |

剩下的 16 句錯誤：

- **代名詞**仍是弱點（她／它／牠 → 他），字元 n-gram 看不到「媽媽……她」這種隔好幾個字的關係。
- 同音詞：人事→人是、發聲→發生、禁行→進行、三斤→三金、背→被。
- 有幾句是**兩種寫法都通用**：宣佈／宣布、唸／念、姐姐／姊姊。目前評分仍算錯，兩邊都用同一標準。

這仍是 Python 原型：每句約 12 毫秒，模型檔 381 MB，還不能直接放進輸入法。

## 第 3 步：Mac 輸入法

以[小麥注音](https://github.com/openvanilla/McBopomofo)（MIT 授權，2026-09-29 版 `be6564a`）為基礎，放在 `mac/McBopomofo/`。改動如下：

- `Source/Engine/gramambular2/contextual_walk.*`：整句選字演算法。用語言模型為整句打分，同時決定斷詞和每段要用哪個詞。使用者手動選過的字一律保留。
- `Source/Engine/KenLMContextModel.*`：語言模型的讀取與評分，使用內嵌的 KenLM 查詢程式碼（`ThirdParty/kenlm`，LGPL-2.1）。
- `Source/KeyHandler.mm`：在「小麥注音」模式下改用整句選字。找不到模型或關閉設定時，自動退回原本的選字方式。「簡易注音」模式（逐字選字）不受影響。
- `Source/Data/context-lm/`：64 MB 的語言模型，切成三個檔案存放，建置時自動合併。

### 成績（同一份小麥注音詞庫，整句正確率）

| | 小麥注音原本的選字 | 整句選字（64 MB 模型） | 整句選字（381 MB 模型） |
|---|---:|---:|---:|
| 測試集 206 句 | 76.7% | 90.8% | 92.7% |
| 　同音詞 51 句 | 58.8% | 82.4% | 92.2% |
| 開發集 600 句 | 48.5% | 81.0% | 82.5% |

模型大小依開發集的結果決定：64 MB 版只比 381 MB 版低 1.5 個百分點。但在測試集的同音詞上差距較大（82.4% 對 92.2%），之後可以考慮提供大模型作為選項。

速度：每次重新選字平均約 2 毫秒、最慢約 6 毫秒（Linux，單執行緒）。

### 在 Mac 上安裝

需求：macOS 14.7 以上、Xcode 15.3 以上、Python 3（產生詞庫用）。

```sh
git clone https://github.com/7500zw-cloud/Zhuyin-input-method.git
cd Zhuyin-input-method
git checkout claude/zhuyin-ime-similar-char-recognition-47ah5e
open mac/McBopomofo/McBopomofo.xcodeproj
```

1. 在 Xcode 上方選擇 **McBopomofoInstaller** scheme，按 ⌘R 執行，畫面出現安裝程式後按「安裝」。
2. 到「系統設定 → 鍵盤 → 文字輸入 → 輸入方式」，按「+」加入「小麥注音」。
3. 如果 Xcode 要求簽署，在 McBopomofo 與 McBopomofoInstaller 兩個 target 的「Signing & Capabilities」選擇 *Sign to Run Locally*。

注意：

- 這個版本與官方小麥注音使用相同的 bundle ID，安裝後會**取代**已安裝的官方小麥注音。要換回官方版，重新安裝官方版即可。
- 同一次登入期間反覆安裝太多次，macOS 可能不再重新啟動輸入法；登出再登入即可。

關閉整句選字（改回小麥注音原本的選字方式），在終端機執行：

```sh
defaults write org.openvanilla.inputmethod.McBopomofo ContextualConversionEnabled -bool false
```

改回 `true` 即可重新開啟。切換後要重新選一次輸入法，或登出再登入。

### 在 Linux 上測試引擎

```sh
cmake -S mac/McBopomofo/Source -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
make -C mac/McBopomofo/Source/Data data.txt context-lm.bin
build/Engine/ContextualWalkEval mac/McBopomofo/Source/Data/data.txt \
  mac/McBopomofo/Source/Data/context-lm.bin < data/testset.tsv > out.tsv
python3 tools/evaluate.py out.tsv
```

不加模型參數時，`ContextualWalkEval` 會使用小麥注音原本的選字方式。

## 限制

- 句子由開發者撰寫，206 句還不算大，每句約占 0.5%，小差距不具意義。之後會加入真實使用中收集到的錯誤。
- 讀音由程式依新酷音詞庫自動標注，經人工逐句檢查，仍可能有誤。
- 代名詞有時本來就無法從語境判斷，該分類只放語境明確的句子。

## 授權

- 本專案的工具與測試集：待定。
- `mac/McBopomofo/`：MIT（小麥注音原作者與本專案貢獻者）。
- `mac/McBopomofo/Source/Engine/ThirdParty/kenlm/`：LGPL-2.1。
- 語言模型 `context-lm`：CC BY-SA 3.0（衍生自維基百科）。
- 新酷音詞庫（LGPL-2.1-or-later）只在本機下載使用，不收進本專案。
