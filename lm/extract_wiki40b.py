#!/usr/bin/env python3
"""把 Wiki40B（TFRecord 格式）轉成字元語言模型的訓練文字。

不依賴 TensorFlow：自行解析 TFRecord 外框與 tf.train.Example 的 protobuf。
輸出每行一個子句，字與字之間以空白分隔；非中文的連續片段（數字、英文）合併成 <x>。

    python3 lm/extract_wiki40b.py corpus/wiki40b/*.tfrecord* > corpus/wiki.txt
"""

import re
import struct
import sys


def records(path):
    with open(path, "rb") as f:
        while header := f.read(12):
            (length,) = struct.unpack("<Q", header[:8])
            data = f.read(length)
            f.read(4)  # data crc
            yield data


def varint(buf, i):
    shift = result = 0
    while True:
        b = buf[i]
        i += 1
        result |= (b & 0x7F) << shift
        if b < 0x80:
            return result, i
        shift += 7


def fields(buf):
    """逐一回傳 (欄位編號, 值)；只處理 varint 與 length-delimited。"""
    i = 0
    while i < len(buf):
        key, i = varint(buf, i)
        num, wire = key >> 3, key & 7
        if wire == 0:
            val, i = varint(buf, i)
        elif wire == 2:
            n, i = varint(buf, i)
            val, i = buf[i:i + n], i + n
        else:
            raise ValueError(f"unsupported wire type {wire}")
        yield num, val


def example_text(data):
    # Example{1: Features{1: map<string, Feature>}}; Feature{1: BytesList{1: bytes}}
    for _, features in fields(data):
        for _, entry in fields(features):
            kv = dict(fields(entry))
            if kv.get(1) == b"text":
                bytes_list = dict(fields(kv[2]))[1]
                return dict(fields(bytes_list))[1].decode("utf-8")
    return ""


CJK = r"㐀-䶿一-鿿豈-﫿"
CLAUSE_SPLIT = re.compile(r"[，。！？；：、「」『』（）《》〈〉…—,.!?;:()\[\]\"'\s]+")
NON_CJK = re.compile(f"[^{CJK}]+")


def paragraphs(text):
    """只取內文段落，略過條目與章節標題（標題常未轉成繁體）。"""
    lines = text.split("\n")
    for prev, line in zip(lines, lines[1:]):
        if prev == "_START_PARAGRAPH_":
            yield from line.split("_NEWLINE_")


def clauses(text):
    for part in paragraphs(text):
        yield from tokenize(part)


def tokenize(part):
    """把一段文字切成子句，每個子句是以空白分隔的字元序列。"""
    for clause in CLAUSE_SPLIT.split(part):
        tokens = []
        for piece in re.split(f"([{CJK}]+)", clause):
            if not piece:
                continue
            if re.match(f"[{CJK}]", piece):
                tokens.extend(piece)
            else:
                tokens.append("<x>")
        if sum(t != "<x>" for t in tokens) >= 2:
            yield " ".join(tokens)


def main():
    import opencc

    # Wiki40B zh-tw 偶有殘留簡體字，統一轉成台灣正體
    to_tw = opencc.OpenCC("s2tw")
    out = sys.stdout
    for path in sys.argv[1:]:
        for data in records(path):
            for c in clauses(to_tw.convert(example_text(data))):
                out.write(c + "\n")


if __name__ == "__main__":
    main()
