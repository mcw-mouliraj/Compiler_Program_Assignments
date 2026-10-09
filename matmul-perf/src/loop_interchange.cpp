#include "matmul.h"
#include <algorithm>

// swap k and j so the innermost loop walks both B and C row-wise
// (stride 1 instead of stride N) - same math, better locality.
void matmulInterchange(const Matrix &A, const Matrix &B, Matrix &C) {
  std::fill(C.begin(), C.end(), 0.0f);

  for (int i = 0; i < N; ++i) {
    for (int k = 0; k < N; ++k) {
      float a = A[i * N + k];
      for (int j = 0; j < N; ++j)
        C[i * N + j] += a * B[k * N + j];
    }
  }
}
