#!/usr/bin/env python3
"""從未參與訓練的語料組出多子句段落（每段 4 個子句，以「，」相連），用來檢查跨子句的選字規則。

每個來源取 400 段，注音以 tools/annotate.py 的方法自動標注，子句之間放 _punctuation_<。

    python3 lm/make_paragraphs.py corpus/wiki_heldout.txt corpus/ptt_heldout.txt > paragraphs.tsv
"""

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from annotate import annotate  # noqa: E402
from testset import load_chewing  # noqa: E402


def main():
    chars, phrases = load_chewing()
    rng = random.Random(7)
    out = ["id\tcategory\ttext\tzhuyin\tfocus\ttrap\tnote"]
    for tag, path in (("wiki", sys.argv[1]), ("ptt", sys.argv[2])):
        lines = [l.split() for l in open(path, encoding="utf-8")]
        n = tries = 0
        while n < 400 and tries < 20000:
            tries += 1
            i = rng.randrange(len(lines) - 4)
            chunk = lines[i:i + 4]
            if any("<x>" in c or not (3 <= len(c) <= 14) for c in chunk):
                continue
            texts = ["".join(c) for c in chunk]
            try:
                syl = [annotate(t, chars, phrases)[0] for t in texts]
            except ValueError:
                continue
            n += 1
            zhuyin = " _punctuation_< ".join(" ".join(s) for s in syl)
            out.append(f"{tag}-{n:03d}\t{tag}\t{'，'.join(texts)}\t{zhuyin}\t\t\t")
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
