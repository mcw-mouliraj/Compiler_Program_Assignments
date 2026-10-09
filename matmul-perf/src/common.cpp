#include "matmul.h"
#include <cmath>
#include <random>

void fillRandom(Matrix &M, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);
  for (float &x : M)
    x = dist(rng);
}

bool nearlyEqual(const Matrix &A, const Matrix &B, float tol) {
  for (size_t i = 0; i < A.size(); ++i)
    if (std::fabs(A[i] - B[i]) > tol)
      return false;
  return true;
}
