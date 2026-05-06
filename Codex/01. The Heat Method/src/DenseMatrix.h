#pragma once

#include <vector>

namespace HeatDemo
{
struct DenseMatrix
{
    int n = 0;
    std::vector<double> data;

    void Resize(int size);
    void SetZero();
    double& At(int row, int col);
    double At(int row, int col) const;
    bool IsSquareSize(int expected) const;
    double MaxSymmetryError() const;
    double MaxAbsRowSum() const;
};
}
