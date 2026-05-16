#!/usr/bin/env bash
# Compiles the existing C++ backends from ../Human_Solution into ./build
# using clang++ on macOS. We supply a local bits/stdc++.h shim so the
# original sources are not modified.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SRC_DIR="$ROOT/Human_Solution"
OUT_DIR="$HERE/build"
SHIM_DIR="$HERE/cpp"

mkdir -p "$OUT_DIR"

COMMON_FLAGS=(
  -std=c++17
  -O2
  -Wno-unknown-pragmas
  -Wno-deprecated-declarations
  -I "$SHIM_DIR"
  -I "$SRC_DIR/include"
  -I "$SRC_DIR"
)

CXX="${CXX:-clang++}"

echo "Building baseline -> $OUT_DIR/baseline"
"$CXX" "${COMMON_FLAGS[@]}" "$SRC_DIR/baseline.cpp" -o "$OUT_DIR/baseline"

echo "Building improved -> $OUT_DIR/improved"
"$CXX" "${COMMON_FLAGS[@]}" "$SRC_DIR/improved.cpp" -o "$OUT_DIR/improved"

# AI backend: a tiny adapter (client/cpp/ai_adapter.cpp) pulls in
# AI_Solution/main.cpp unchanged and exposes the ScarcityAwareGreedySolver
# via the same stdin/stdout contract the other two backends use.
echo "Building ai -> $OUT_DIR/ai"
"$CXX" "${COMMON_FLAGS[@]}" "$SHIM_DIR/ai_adapter.cpp" -o "$OUT_DIR/ai"

echo "Done."
