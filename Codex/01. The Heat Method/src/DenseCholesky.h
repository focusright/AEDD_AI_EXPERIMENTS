#pragma once

#include "DenseMatrix.h"

#include <string>
#include <vector>

namespace HeatDemo
{
class DenseCholesky
{
public:
    bool Factor(const DenseMatrix& matrix);
    bool Solve(const std::vector<double>& rhs, std::vector<double>& x) const;

    int Size() const { return n; }
    bool IsFactored() const { return factored; }
    const std::string& LastError() const { return lastError; }

private:
    int n = 0;
    bool factored = false;
    std::vector<double> lower;
    std::string lastError;

    double L(int row, int col) const;
    double& L(int row, int col);
};
}
