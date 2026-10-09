#include "matmul.h"
#include <algorithm>

// 32x32 blocks keep a chunk of A/B/C resident in cache
static constexpr int TILE = 32;

void matmulTiled(const Matrix &A, const Matrix &B, Matrix &C) {
  std::fill(C.begin(), C.end(), 0.0f);

  for (int ii = 0; ii < N; ii += TILE) {
    int iEnd = std::min(ii + TILE, N);

    for (int kk = 0; kk < N; kk += TILE) {
      int kEnd = std::min(kk + TILE, N);

      for (int jj = 0; jj < N; jj += TILE) {
        int jEnd = std::min(jj + TILE, N);

        for (int i = ii; i < iEnd; ++i) {
          for (int k = kk; k < kEnd; ++k) {
            float a = A[i * N + k];
            for (int j = jj; j < jEnd; ++j)
              C[i * N + j] += a * B[k * N + j];
          }
        }
      }
    }
  }
}
