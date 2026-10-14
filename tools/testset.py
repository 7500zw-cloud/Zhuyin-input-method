"""測試集的共用工具：讀寫 data/testset.tsv、注音格式檢查、新酷音詞庫載入。"""

import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTSET = ROOT / "data" / "testset.tsv"
CHEWING_DICT = ROOT / "third_party" / "libchewing-data" / "dict" / "chewing"

FIELDS = ["id", "category", "text", "zhuyin", "focus", "trap", "note"]

CATEGORIES = {
    "seg": "斷詞歧義",
    "homo": "同音詞（靠語境）",
    "func": "虛詞（在再、做作、的得、已以）",
    "pron": "代名詞（他她它牠）",
    "long": "一般語境",
    "term": "科技／新詞",
    "name": "專有名詞",
    "chat": "口語對話",
    "num": "數字量詞",
    "base": "日常基本句",
}

# 一個注音音節：聲母? 介音? 韻母? 聲調?（一聲不標）
_INITIALS = "ㄅㄆㄇㄈㄉㄊㄋㄌㄍㄎㄏㄐㄑㄒㄓㄔㄕㄖㄗㄘㄙ"
_MEDIALS = "ㄧㄨㄩ"
_FINALS = "ㄚㄛㄜㄝㄞㄟㄠㄡㄢㄣㄤㄥㄦ"
_TONES = "ˊˇˋ˙"
SYLLABLE_RE = re.compile(
    f"^[{_INITIALS}]?[{_MEDIALS}]?[{_FINALS}]?[{_TONES}]?$"
)

# 異體字視為相同，避免把「台／臺」這類都正確的寫法算成錯
VARIANTS = str.maketrans({"臺": "台", "裏": "裡", "週": "周", "着": "著", "爲": "為"})


def normalize(text):
    return text.translate(VARIANTS)


def is_syllable(s):
    return bool(s) and s not in _TONES and SYLLABLE_RE.match(s) is not None


def load_testset(path=TESTSET):
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def save_testset(rows, path=TESTSET):
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, FIELDS, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)


def _read_csv_dict(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            text, freq, reading = line.rstrip("\n").split(",")
            yield text, int(freq), tuple(reading.split(" "))


def load_chewing(dict_dir=CHEWING_DICT):
    """回傳 (chars, phrases)：
    chars[字] = 該字所有讀音的集合
    phrases[詞] = [(讀音 tuple, 頻率), ...]
    """
    if not dict_dir.exists():
        raise SystemExit("找不到新酷音詞庫，請先執行 tools/fetch_chewing_data.sh")
    chars, phrases = {}, {}
    for name in ("word.csv", "mini.csv"):
        for text, _, reading in _read_csv_dict(dict_dir / name):
            if len(text) == 1 and len(reading) == 1:
                chars.setdefault(text, set()).add(reading[0])
    for name in ("tsi.csv", "alt.csv"):
        for text, freq, reading in _read_csv_dict(dict_dir / name):
            if len(text) == len(reading):
                phrases.setdefault(text, []).append((reading, freq))
    return chars, phrases
