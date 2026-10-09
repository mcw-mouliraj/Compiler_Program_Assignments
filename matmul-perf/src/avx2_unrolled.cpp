#include "matmul.h"
#include <immintrin.h>

// unroll k by 4, 4 separate accumulators so the FMAs don't
// serialize on one running sum
void matmulAVX2Unrolled(const Matrix &A, const Matrix &B, Matrix &C) {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; j += 8) {
      __m256 sum0 = _mm256_setzero_ps();
      __m256 sum1 = _mm256_setzero_ps();
      __m256 sum2 = _mm256_setzero_ps();
      __m256 sum3 = _mm256_setzero_ps();

      int k = 0;
      for (; k <= N - 4; k += 4) {
        sum0 = _mm256_fmadd_ps(_mm256_set1_ps(A[i * N + k]),
                                _mm256_loadu_ps(&B[k * N + j]), sum0);
        sum1 = _mm256_fmadd_ps(_mm256_set1_ps(A[i * N + k + 1]),
                                _mm256_loadu_ps(&B[(k + 1) * N + j]), sum1);
        sum2 = _mm256_fmadd_ps(_mm256_set1_ps(A[i * N + k + 2]),
                                _mm256_loadu_ps(&B[(k + 2) * N + j]), sum2);
        sum3 = _mm256_fmadd_ps(_mm256_set1_ps(A[i * N + k + 3]),
                                _mm256_loadu_ps(&B[(k + 3) * N + j]), sum3);
      }

      __m256 sum = _mm256_add_ps(_mm256_add_ps(sum0, sum1),
                                  _mm256_add_ps(sum2, sum3));

      for (; k < N; ++k) // leftover if N isn't a multiple of 4
        sum = _mm256_fmadd_ps(_mm256_set1_ps(A[i * N + k]),
                               _mm256_loadu_ps(&B[k * N + j]), sum);

      _mm256_storeu_ps(&C[i * N + j], sum);
    }
  }
}
