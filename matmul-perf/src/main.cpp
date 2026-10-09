#include "matmul.h"
#include <chrono>
#include <iomanip>
#include <iostream>

namespace {

constexpr int ITERATIONS = 3;

// times func, checks against the reference first, prints ms + GFLOPS
double run(const char *name, void (*func)(const Matrix &, const Matrix &, Matrix &),
           const Matrix &A, const Matrix &B, Matrix &C, const Matrix &reference) {
  func(A, B, C);
  if (!nearlyEqual(C, reference)) {
    std::cerr << name << ": result does not match the naive reference\n";
    return -1.0;
  }

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ITERATIONS; ++i)
    func(A, B, C);
  auto end = std::chrono::high_resolution_clock::now();

  double ms = std::chrono::duration<double, std::milli>(end - start).count() / ITERATIONS;
  double gflops = (2.0 * N * N * N) / (ms * 1e6);

  std::cout << std::left << std::setw(20) << name << std::right << std::setw(10)
            << std::fixed << std::setprecision(2) << ms << " ms " << std::setw(10)
            << gflops << " GFLOPS\n";
  return ms;
}

} // namespace

int main() {
  std::cout << "matrix multiplication, " << N << "x" << N << " float\n\n";

  Matrix A(N * N), B(N * N), C(N * N), reference(N * N);
  fillRandom(A, 1);
  fillRandom(B, 2);

  matmulNaive(A, B, reference);

  std::cout << std::left << std::setw(20) << "implementation" << std::right
            << std::setw(13) << "time" << std::setw(16) << "throughput" << "\n";
  std::cout << std::string(49, '-') << "\n";

  double tNaive = run("naive (i-j-k)", matmulNaive, A, B, C, reference);
  double tInter = run("loop interchange", matmulInterchange, A, B, C, reference);
  double tTiled = run("loop tiling", matmulTiled, A, B, C, reference);
  double tAVX = run("AVX2", matmulAVX2, A, B, C, reference);
  double tAVXUnrolled = run("AVX2 + unroll", matmulAVX2Unrolled, A, B, C, reference);

  std::cout << std::string(49, '-') << "\n\nspeedup vs naive:\n";
  if (tNaive > 0) {
    std::cout << std::setprecision(2);
    std::cout << "  loop interchange : " << tNaive / tInter << "x\n";
    std::cout << "  loop tiling       : " << tNaive / tTiled << "x\n";
    std::cout << "  AVX2              : " << tNaive / tAVX << "x\n";
    std::cout << "  AVX2 + unroll     : " << tNaive / tAVXUnrolled << "x\n";
  }

  return 0;
}
