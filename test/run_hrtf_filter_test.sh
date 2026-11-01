#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

OUT="$(mktemp -d "${TMPDIR:-/tmp}/hrtf_filter_test.XXXXXX")"
trap 'rm -f "$OUT/hrtf_filter_test" "$OUT/hrtf_filter_test.exe" "$OUT/synthetic.bin"; rmdir "$OUT"' EXIT
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -pthread -DWITH_NULL -DNOMINMAX \
    -I src -I src/soloud/include -I src/soloud/src \
    -o "$OUT/hrtf_filter_test" test/hrtf_filter_test.cpp \
    src/filters/soloud_hrtf_filter.cpp src/soloud/src/core/*.cpp \
    src/soloud/src/backend/null/soloud_null.cpp
"$OUT/hrtf_filter_test" "$OUT/synthetic.bin"
