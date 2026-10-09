#!/usr/bin/env bash
# builds and runs the matmul benchmark
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

CXX=${CXX:-g++}

mkdir -p out
"$CXX" -std=c++17 -O3 -mavx2 -mfma -Iinclude \
    src/*.cpp \
    -o out/matmul-bench

echo "built out/matmul-bench, running:"
echo
./out/matmul-bench
