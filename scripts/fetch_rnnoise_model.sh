#!/usr/bin/env bash
# Downloads the RNNoise 0.2 model (the "stronger noise removal" AI) into
# app/src/main/cpp/third_party/rnnoise2/src. Run once before building.
# Without it the build automatically uses the smaller RNNoise 0.1.1 model.
set -euo pipefail

DIR="$(cd "$(dirname "$0")/.." && pwd)/app/src/main/cpp/third_party/rnnoise2"
VERSION="$(tr -d '[:space:]' < "$DIR/model_version")"
FILE="rnnoise_data-${VERSION}.tar.gz"

if [ -f "$DIR/src/rnnoise_data.c" ]; then
  echo "RNNoise model already present."
  exit 0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
echo "Downloading $FILE"
curl -fsSL --retry 3 -o "$TMP/$FILE" "https://media.xiph.org/rnnoise/models/$FILE"
echo "sha256: $(sha256sum "$TMP/$FILE" | cut -d' ' -f1)"
tar -xzf "$TMP/$FILE" -C "$TMP"
cp "$TMP/src/rnnoise_data.c" "$TMP/src/rnnoise_data.h" "$DIR/src/"
echo "RNNoise model installed ($(du -h "$DIR/src/rnnoise_data.c" | cut -f1) of source)."
