#include "DenseCholesky.h"

#include <cassert>
#include <cmath>
#include <sstream>

namespace HeatDemo
{
double DenseCholesky::L(int row, int col) const
{
    return lower[static_cast<size_t>(row) * static_cast<size_t>(n) + static_cast<size_t>(col)];
}

double& DenseCholesky::L(int row, int col)
{
    return lower[static_cast<size_t>(row) * static_cast<size_t>(n) + static_cast<size_t>(col)];
}

bool DenseCholesky::Factor(const DenseMatrix& matrix)
{
    n = matrix.n;
    factored = false;
    lastError.clear();
    lower.assign(static_cast<size_t>(n) * static_cast<size_t>(n), 0.0);

    if (!matrix.IsSquareSize(n)) {
        lastError = "matrix storage size does not match matrix dimension";
        return false;
    }

    constexpr double diagonalTolerance = 1e-14;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j <= i; ++j) {
            double sum = matrix.At(i, j);
            for (int k = 0; k < j; ++k) {
                sum -= L(i, k) * L(j, k);
            }

            if (i == j) {
                if (!std::isfinite(sum) || sum <= diagonalTolerance) {
                    std::ostringstream stream;
                    stream << "non-positive Cholesky diagonal at row " << i << ": " << sum;
                    lastError = stream.str();
                    return false;
                }
                L(i, j) = std::sqrt(sum);
            } else {
                const double diagonal = L(j, j);
                if (std::abs(diagonal) <= diagonalTolerance) {
                    std::ostringstream stream;
                    stream << "near-zero Cholesky pivot at row " << j;
                    lastError = stream.str();
                    return false;
                }
                L(i, j) = sum / diagonal;
            }
        }
    }

    factored = true;
    return true;
}

bool DenseCholesky::Solve(const std::vector<double>& rhs, std::vector<double>& x) const
{
    if (!factored || static_cast<int>(rhs.size()) != n) {
        return false;
    }

    std::vector<double> y(static_cast<size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        double sum = rhs[static_cast<size_t>(i)];
        for (int k = 0; k < i; ++k) {
            sum -= L(i, k) * y[static_cast<size_t>(k)];
        }
        y[static_cast<size_t>(i)] = sum / L(i, i);
    }

    x.assign(static_cast<size_t>(n), 0.0);
    for (int i = n - 1; i >= 0; --i) {
        double sum = y[static_cast<size_t>(i)];
        for (int k = i + 1; k < n; ++k) {
            sum -= L(k, i) * x[static_cast<size_t>(k)];
        }
        x[static_cast<size_t>(i)] = sum / L(i, i);
    }

    return true;
}
}
