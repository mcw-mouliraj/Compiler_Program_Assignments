#!/usr/bin/env bash
# Installs the Mouli* LLVM passes into an llvm-project checkout and
# (optionally) builds `opt` with them.
#
# Usage:
#   ./install.sh /path/to/llvm-project [/path/to/build-dir]
#
# If a build dir is given and already configured with CMake+Ninja, the
# script will also run the build at the end. If it's omitted, the script
# only copies files and patches the registration points -- you build it
# yourself afterwards.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
  echo "usage: $0 /path/to/llvm-project [/path/to/build-dir]"
  exit 1
fi

LLVM_ROOT="$1"
BUILD_DIR="${2:-}"

UTILS_INC="$LLVM_ROOT/llvm/include/llvm/Transforms/Utils"
UTILS_LIB="$LLVM_ROOT/llvm/lib/Transforms/Utils"
CMAKE_FILE="$UTILS_LIB/CMakeLists.txt"
PASSBUILDER_FILE="$LLVM_ROOT/llvm/lib/Passes/PassBuilder.cpp"
REGISTRY_FILE="$LLVM_ROOT/llvm/lib/Passes/PassRegistry.def"

for f in "$CMAKE_FILE" "$PASSBUILDER_FILE" "$REGISTRY_FILE"; do
  if [ ! -f "$f" ]; then
    echo "error: expected file not found: $f"
    echo "(is $LLVM_ROOT really an llvm-project checkout?)"
    exit 1
  fi
done

echo "==> copying headers into $UTILS_INC"
cp -v "$SCRIPT_DIR"/include/*.h "$UTILS_INC/"

echo "==> copying sources into $UTILS_LIB"
cp -v "$SCRIPT_DIR"/lib/*.cpp "$UTILS_LIB/"

# --- CMakeLists.txt: add each .cpp to the source list, once ---------------
echo "==> patching $CMAKE_FILE"
for cpp in MouliCFSCCP.cpp MouliCSE.cpp MouliDCE.cpp MouliSR.cpp; do
  if grep -qF "$cpp" "$CMAKE_FILE"; then
    echo "    $cpp already listed, skipping"
  else
    sed -i "s/^\(\s*HelloWorld\.cpp\)/\1\n  $cpp/" "$CMAKE_FILE"
    echo "    added $cpp"
  fi
done

# --- PassBuilder.cpp: add each header include, once ------------------------
echo "==> patching $PASSBUILDER_FILE"
for hdr in MouliCFSCCP.h MouliCSE.h MouliDCE.h MouliSR.h; do
  inc="#include \"llvm/Transforms/Utils/$hdr\""
  if grep -qF "$inc" "$PASSBUILDER_FILE"; then
    echo "    $hdr already included, skipping"
  else
    sed -i "\#include \"llvm/Transforms/Utils/HelloWorld.h\"#a $inc" "$PASSBUILDER_FILE"
    echo "    added include for $hdr"
  fi
done

# --- PassRegistry.def: register each pass, once -----------------------------
echo "==> patching $REGISTRY_FILE"
declare -A PASS_LINES=(
  [mouli-cf-sccp]='FUNCTION_PASS("mouli-cf-sccp", MouliCFSCCPPass())'
  [mouli-cse]='FUNCTION_PASS("mouli-cse", MouliCSEPass())'
  [mouli-dce]='FUNCTION_PASS("mouli-dce", MouliDCEPass())'
  [mouli-sr]='FUNCTION_PASS("mouli-sr", MouliSRPass())'
)
for name in "${!PASS_LINES[@]}"; do
  line="${PASS_LINES[$name]}"
  if grep -qF "$line" "$REGISTRY_FILE"; then
    echo "    $name already registered, skipping"
  else
    sed -i "\#FUNCTION_PASS(\"helloworld\", HelloWorldPass())#a $line" "$REGISTRY_FILE"
    echo "    registered $name"
  fi
done

# --- sanity check: these passes need CondBrInst/UncondBrInst ---------------
if ! grep -q "class CondBrInst" "$LLVM_ROOT/llvm/include/llvm/IR/Instructions.h"; then
  echo
  echo "WARNING: this checkout's Instructions.h doesn't define CondBrInst."
  echo "MouliCFSCCP.cpp needs the course's modified LLVM fork (the one that"
  echo "splits BranchInst into UncondBrInst/CondBrInst). Building against a"
  echo "stock llvm-project will fail on that file specifically."
fi

echo
echo "==> done patching source tree."

if [ -n "$BUILD_DIR" ]; then
  if [ ! -d "$BUILD_DIR" ]; then
    echo "error: build dir not found: $BUILD_DIR"
    exit 1
  fi
  echo "==> building opt in $BUILD_DIR"
  cmake --build "$BUILD_DIR" --target opt
  echo "==> build finished: $BUILD_DIR/bin/opt"
else
  echo "no build dir given -- configure/build it yourself, e.g.:"
  echo "  cmake -S $LLVM_ROOT/llvm -B <build-dir> -G Ninja -DLLVM_ENABLE_PROJECTS=clang"
  echo "  cmake --build <build-dir> --target opt"
fi

echo
echo "try it out, e.g.:"
echo "  opt -passes=mouli-cf-sccp -S input.ll -o -"
echo "  opt -passes=mouli-dce,mouli-cse,mouli-sr -S input.ll -o -"
