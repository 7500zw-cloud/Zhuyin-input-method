#!/bin/sh
# 下載新酷音詞庫（LGPL-2.1-or-later）到 third_party/，僅供標注與驗證讀音，不併入本專案。
set -eu
cd "$(dirname "$0")/.."
DEST=third_party/libchewing-data
REV=${CHEWING_DATA_REV:-c44e81a}
if [ ! -d "$DEST/.git" ]; then
  git clone -q https://github.com/chewing/libchewing-data "$DEST"
fi
git -C "$DEST" checkout -q "$REV"
echo "libchewing-data @ $(git -C "$DEST" rev-parse --short HEAD)"
