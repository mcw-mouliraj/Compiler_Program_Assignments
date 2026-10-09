#ifndef MATMUL_H
#define MATMUL_H

#include <vector>

// square matrix size used throughout the benchmark
constexpr int N = 1024;

using Matrix = std::vector<float>;

void fillRandom(Matrix &M, unsigned seed);
bool nearlyEqual(const Matrix &A, const Matrix &B, float tol = 1e-2f);

void matmulNaive(const Matrix &A, const Matrix &B, Matrix &C);
void matmulInterchange(const Matrix &A, const Matrix &B, Matrix &C);
void matmulTiled(const Matrix &A, const Matrix &B, Matrix &C);
void matmulAVX2(const Matrix &A, const Matrix &B, Matrix &C);
void matmulAVX2Unrolled(const Matrix &A, const Matrix &B, Matrix &C);

#endif
