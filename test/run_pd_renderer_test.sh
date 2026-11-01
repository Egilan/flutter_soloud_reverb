#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

OUT="${TMPDIR:-/tmp}/pd_renderer_test"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -pthread \
    -o "$OUT" test/pd_renderer_test.cpp
"$OUT"
