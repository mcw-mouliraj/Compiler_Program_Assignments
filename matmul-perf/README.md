# Matrix Multiplication Performance Experiments

1024×1024 single-precision matrix multiplication, compared across four
optimization techniques against a naive baseline:

| File | Technique |
|------|-----------|
| `src/naive.cpp` | baseline `i-j-k` triple loop |
| `src/loop_interchange.cpp` | reorder to `i-k-j` so both `B` and `C` are walked row-wise |
| `src/loop_tiling.cpp` | 32×32 blocking so a block of `A`/`B`/`C` stays resident in cache |
| `src/avx2.cpp` | AVX2 intrinsics, 8 floats/register, FMA |
| `src/avx2_unrolled.cpp` | same AVX2 kernel, `k` unrolled ×4 across 4 independent accumulators |

Each technique is its own file; `src/main.cpp` drives all five, validates
every result against the naive output, and times them.

## Build & run

```bash
./build.sh
```

Needs a C++17 compiler with AVX2/FMA support (`g++`/`clang++`). The script
compiles with `-O3 -mavx2 -mfma` and runs the resulting binary.

## Device spec (the machine these numbers were measured on)

- **CPU:** Intel Core i5-1345U (13th Gen), 6 performance+efficiency cores /
  12 threads, up to 1.6 GHz base (boost varies)
- **Cache:** 288 KiB L1d, 192 KiB L1i, 7.5 MiB L2, 12 MiB L3 (shared)
- **RAM:** 16 GB physical (host); benchmark ran inside WSL2, which caps the
  VM at ~7.6 GiB — doesn't matter here since the working set (3 × 1024² ×
  4 bytes ≈ 12 MB) fits well within that
- **OS:** Windows 11 Business (build 26200) running Ubuntu via WSL2
- **Compiler:** g++, `-O3 -mavx2 -mfma`, single-threaded (no OpenMP/threads
  used anywhere)
- **ISA support confirmed:** AVX2, FMA (`/proc/cpuinfo` flags)

## Results

(3 iterations per technique after a warm-up/validation run; see `src/main.cpp`)

```
implementation               time      throughput
-------------------------------------------------
naive (i-j-k)          4944.41 ms       0.43 GFLOPS
loop interchange        153.73 ms      13.97 GFLOPS
loop tiling             209.72 ms      10.24 GFLOPS
AVX2                    693.42 ms       3.10 GFLOPS
AVX2 + unroll           650.10 ms       3.30 GFLOPS
-------------------------------------------------

speedup vs naive:
  loop interchange : 32.16x
  loop tiling       : 23.58x
  AVX2              : 7.13x
  AVX2 + unroll     : 7.61x
```

(Re-run twice; numbers were consistent within ~10%, e.g. loop interchange
measured between 32x-34x, AVX2 between 7.1x-7.4x across runs.)

## Notes on the results

Loop interchange ends up the fastest of all four, ahead of both AVX2
versions by a good margin. Wasn't expecting that going in, so here's
what seems to be going on.

`matmulAVX2` keeps the naive loop order (i-j-k), it just vectorizes the
j loop. So it still reads down a column of B with a stride of N floats
on every k step - a cache line miss basically every time. Making the
arithmetic 8-wide doesn't help much if the loop is mostly waiting on
memory anyway.

`matmulInterchange` doesn't use any SIMD, but it reads B and C
row-wise in the inner loop instead, so the stride problem goes away.
That alone beats both AVX2 kernels here, which says memory access
pattern matters more than vectorization for this kernel on this CPU.

Tiling comes in a bit behind plain interchange. With N=1024 and a
12 MB L3 on this CPU, the interchanged loop is probably already cache
friendly enough that the extra tiling bookkeeping doesn't pay off.
Might flip the other way at a larger N.

AVX2+unroll is a little faster than plain AVX2 since the 4 independent
accumulators avoid serializing the FMAs, but both still have the same
bad stride, so the gap stays small.

Combining interchange/tiling with AVX2 would probably beat all four of
these.

## Files

```
include/matmul.h          shared declarations (Matrix type, N, function signatures)
src/common.cpp            fillRandom, nearlyEqual
src/naive.cpp             baseline
src/loop_interchange.cpp
src/loop_tiling.cpp
src/avx2.cpp
src/avx2_unrolled.cpp
src/main.cpp              benchmark driver / validation / reporting
build.sh
```
