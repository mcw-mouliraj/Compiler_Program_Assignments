#include "matmul.h"
#include <immintrin.h>

// 8 floats per register, broadcast A[i][k] and FMA against B[k][j..j+8]
void matmulAVX2(const Matrix &A, const Matrix &B, Matrix &C) {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; j += 8) {
      __m256 sum = _mm256_setzero_ps();

      for (int k = 0; k < N; ++k) {
        __m256 a = _mm256_set1_ps(A[i * N + k]);
        __m256 b = _mm256_loadu_ps(&B[k * N + j]);
        sum = _mm256_fmadd_ps(a, b, sum);
      }

      _mm256_storeu_ps(&C[i * N + j], sum);
    }
  }
}
