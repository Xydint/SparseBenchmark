#pragma once

#include <Eigen/Sparse>
#include <Eigen/Core>
#include "../common/solver_result.h"
#include "../common/benchmark_config.h"

SolverResult runPardiso(
    const Eigen::SparseMatrix<double>& A,
    const Eigen::VectorXd& b,
    const BenchmarkConfig& cfg);