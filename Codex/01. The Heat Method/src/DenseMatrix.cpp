#include "DenseMatrix.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace HeatDemo
{
void DenseMatrix::Resize(int size)
{
    n = size;
    data.assign(static_cast<size_t>(n) * static_cast<size_t>(n), 0.0);
}

void DenseMatrix::SetZero()
{
    std::fill(data.begin(), data.end(), 0.0);
}

double& DenseMatrix::At(int row, int col)
{
    assert(row >= 0 && row < n);
    assert(col >= 0 && col < n);
    return data[static_cast<size_t>(row) * static_cast<size_t>(n) + static_cast<size_t>(col)];
}

double DenseMatrix::At(int row, int col) const
{
    assert(row >= 0 && row < n);
    assert(col >= 0 && col < n);
    return data[static_cast<size_t>(row) * static_cast<size_t>(n) + static_cast<size_t>(col)];
}

bool DenseMatrix::IsSquareSize(int expected) const
{
    return n == expected && data.size() == static_cast<size_t>(expected) * static_cast<size_t>(expected);
}

double DenseMatrix::MaxSymmetryError() const
{
    double maxError = 0.0;
    for (int r = 0; r < n; ++r) {
        for (int c = r + 1; c < n; ++c) {
            maxError = std::max(maxError, std::abs(At(r, c) - At(c, r)));
        }
    }
    return maxError;
}

double DenseMatrix::MaxAbsRowSum() const
{
    double maxRowSum = 0.0;
    for (int r = 0; r < n; ++r) {
        double rowSum = 0.0;
        for (int c = 0; c < n; ++c) {
            rowSum += At(r, c);
        }
        maxRowSum = std::max(maxRowSum, std::abs(rowSum));
    }
    return maxRowSum;
}
}
