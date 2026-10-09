#include "matmul.h"

// baseline: i-j-k order. B[k][j] is walked column-wise (stride N),
// so this is the worst case for cache locality.
void matmulNaive(const Matrix &A, const Matrix &B, Matrix &C) {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      float sum = 0.0f;
      for (int k = 0; k < N; ++k)
        sum += A[i * N + k] * B[k * N + j];
      C[i * N + j] = sum;
    }
  }
}
