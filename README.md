# Mouli* LLVM Passes

Four hand-written LLVM function passes:

| Pass name        | File               | What it does |
|-------------------|--------------------|--------------|
| `mouli-cf-sccp`   | `MouliCFSCCP.cpp`  | Constant folding + conditional constant propagation (mini SCCP): folds constant expressions/phis, turns branches with a known condition into unconditional jumps, and deletes the now-unreachable blocks. |
| `mouli-dce`       | `MouliDCE.cpp`     | Dead code elimination: repeatedly removes instructions with no uses, no side effects, that aren't terminators. |
| `mouli-cse`       | `MouliCSE.cpp`     | Common subexpression elimination: walks the dominator tree with a scoped hash table, reusing an earlier identical instruction instead of recomputing it. |
| `mouli-sr`        | `MouliSR.cpp`      | Strength reduction: rewrites `mul x, 2^k` into `shl x, k`. |

All four are self-contained (no dependency on each other) and can be run individually or chained together with `opt -passes=...`.

## Requirements

- This must be built against the **course-provided LLVM fork**, not a stock `llvm-project` checkout. In particular, `MouliCFSCCP.cpp` uses `CondBrInst`, which only exists in this fork's `Instructions.h` (it splits the usual `BranchInst` into separate `UncondBrInst` / `CondBrInst` classes). `install.sh` checks for this and warns if it's missing.
- A working CMake + Ninja build of that LLVM checkout (the same one you'd use to build `opt` normally).

## What's in this repo

```
include/
  MouliCFSCCP.h
  MouliCSE.h
  MouliDCE.h
  MouliSR.h
lib/
  MouliCFSCCP.cpp
  MouliCSE.cpp
  MouliDCE.cpp
  MouliSR.cpp
testcases/
  mouli_cf_sccp_const_fold.c
  mouli_cf_sccp_algebraic.c
  mouli_cf_sccp_copy_prop.c
  mouli_cse_basic.c
  mouli_dce_redundant_assign.c
  mouli_dce_combined.c
  mouli_sr_pow2.c
install.sh
README.md
```

The `.h` files belong in `llvm/include/llvm/Transforms/Utils/`, the `.cpp` files in
`llvm/lib/Transforms/Utils/`. Beyond copying the files in, three existing LLVM files
need small additions so the passes are registered with `opt`:

- `llvm/lib/Transforms/Utils/CMakeLists.txt` — add each `.cpp` to the source list.
- `llvm/lib/Passes/PassBuilder.cpp` — `#include` each new header.
- `llvm/lib/Passes/PassRegistry.def` — one `FUNCTION_PASS(...)` line per pass.

`install.sh` does all of the above for you.

## Quick start (automated)

```bash
./install.sh /path/to/llvm-project
```

This copies the 8 files into place and patches the three registration files
(safe to re-run — it skips anything already present). Then build `opt`
yourself as usual, e.g.:

```bash
cmake --build /path/to/build --target opt
```

Or, if your build directory is already configured, hand it to the script
and it'll build for you too:

```bash
./install.sh /path/to/llvm-project /path/to/build
```

## Manual install (if you'd rather not run a script)

1. Copy `include/*.h` into `llvm/include/llvm/Transforms/Utils/`.
2. Copy `lib/*.cpp` into `llvm/lib/Transforms/Utils/`.
3. In `llvm/lib/Transforms/Utils/CMakeLists.txt`, add the four `.cpp`
   filenames to the source list (anywhere in the list works).
4. In `llvm/lib/Passes/PassBuilder.cpp`, add:
   ```cpp
   #include "llvm/Transforms/Utils/MouliCFSCCP.h"
   #include "llvm/Transforms/Utils/MouliCSE.h"
   #include "llvm/Transforms/Utils/MouliDCE.h"
   #include "llvm/Transforms/Utils/MouliSR.h"
   ```
5. In `llvm/lib/Passes/PassRegistry.def`, add:
   ```cpp
   FUNCTION_PASS("mouli-cf-sccp", MouliCFSCCPPass())
   FUNCTION_PASS("mouli-cse", MouliCSEPass())
   FUNCTION_PASS("mouli-dce", MouliDCEPass())
   FUNCTION_PASS("mouli-sr", MouliSRPass())
   ```
6. Rebuild `opt`.

## Trying it out

```bash
opt -passes=mouli-cf-sccp -S input.ll -o -
opt -passes=mouli-dce,mouli-cse,mouli-sr -S input.ll -o -
```

Each pass prints a line to stderr for every change it makes (folded
instruction, collapsed branch, removed block, reused expression, etc.),
so `2>&1` will show what happened alongside the resulting IR.

## Testcases

`testcases/*.c` are small C functions, one per technique. Important: these
passes work on LLVM IR values, not on memory (`alloca`/`load`/`store`).
`clang -O0` output is all memory ops, so **`mem2reg` has to run first** to
promote locals into real SSA values before `mouli-dce`, `mouli-cse`, or
`mouli-cf-sccp` can see anything to do. `mouli-sr` is the one exception —
it matches on the `mul` instruction directly, so it works even without
`mem2reg`. Also pass `-Xclang -disable-O0-optnone` to clang, otherwise
`-O0` marks the function `optnone` and `opt` refuses to run most passes
on it at all.

```bash
clang -O0 -S -emit-llvm -Xclang -disable-O0-optnone testcases/NAME.c -o out.ll
opt -passes=mem2reg,mouli-cf-sccp,mouli-cse,mouli-dce -S out.ll -o -
```

| File | Tests | Verified result |
|------|-------|------------------|
| `mouli_cf_sccp_const_fold.c` | constant folding | fully collapses to `ret i32 22` |
| `mouli_cf_sccp_copy_prop.c` | copy propagation through an (uninitialized) local | collapses to `%1 = add i32 undef, 4; ret i32 %1` |
| `mouli_cf_sccp_algebraic.c` | algebraic identities (`x/x`, `x-x`, `x*1`, ...) | **not folded** — see limitation below |
| `mouli_cse_basic.c` | common subexpression elimination | `a+b` computed once, reused for both uses |
| `mouli_dce_redundant_assign.c` | dead/redundant assignments | fully collapses to `ret i32 3` |
| `mouli_dce_combined.c` | const-fold + dce together | fully collapses to `ret i32 7` |
| `mouli_sr_pow2.c` | `mul` by a power of two | `mul x, 2`/`mul x, 8` become `shl x, 1`/`shl x, 3` (works with no `mem2reg`) |

**Known limitation — `mouli_cf_sccp_algebraic.c`:** `mouli-cf-sccp` only
folds an expression when *both* operands are already known constants
(literal or previously proven). It has no notion of algebraic identities
like `x / x == 1` or `x - x == 0` for a symbolic, non-constant `x` — that
needs pattern-matching on instruction shape (like LLVM's real InstCombine
does), not constant propagation. This test case is included to be
transparent about that boundary, not because it's expected to fully
optimize.
