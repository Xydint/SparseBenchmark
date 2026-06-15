#pragma once

#include <Eigen/Sparse>
#include <Eigen/Dense>
#include "../common/solver_result.h"
#include "../common/benchmark_config.h"

SolverResult runMUMPS(
    const Eigen::SparseMatrix<double>& A,
    const Eigen::VectorXd& b,
    const BenchmarkConfig& cfg);