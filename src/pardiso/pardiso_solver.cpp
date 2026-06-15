#include "pardiso_solver.h"
#include <chrono>
#include <stdexcept>
#include <omp.h>
#include <mkl.h>
#include "mem_util.h"
#include <algorithm>   
#include <Eigen/PardisoSupport>

using namespace Eigen;

SolverResult runPardiso(
    const SparseMatrix<double>& A,
    const VectorXd& b,
    const BenchmarkConfig& cfg)
{
    SolverResult result;
    result.solver  = "PARDISO";
    result.threads = cfg.threads;
    result.rows    = A.rows();
    result.cols    = A.cols();
    result.nnz     = A.nonZeros();

    // потоки для MKL PARDISO
    mkl_set_num_threads(cfg.threads);  // управляет Intel OpenMP внутри MKL    

    PardisoLU<SparseMatrix<double>> solver;

    // iparm[10], iparm[12] — масштабирование и matching, оставляем
    solver.pardisoParameterArray()[10] = 1;
    solver.pardisoParameterArray()[12] = 1;

    auto factorStart = std::chrono::high_resolution_clock::now();
    solver.compute(A);
    auto factorEnd = std::chrono::high_resolution_clock::now();
    result.factorTime = std::chrono::duration<double>(factorEnd - factorStart).count();

    if (solver.info() != Success)
        throw std::runtime_error("PARDISO factorization failed!");

    if (solver.info() != Success)
        throw std::runtime_error("PARDISO factorization failed!");

    // IPARM(15)=ip[14] пик анализа (КБ); IPARM(16)+IPARM(17)=ip[15]+ip[16] факторизация (КБ)
    auto& ip = solver.pardisoParameterArray();
    double peakKB = std::max<double>(ip[14], double(ip[15]) + double(ip[16]));
    result.solverMemMB = peakKB / 1024.0;
    
    auto solveStart = std::chrono::high_resolution_clock::now();
    VectorXd x = solver.solve(b);
    auto solveEnd = std::chrono::high_resolution_clock::now();
    result.solveTime = std::chrono::duration<double>(solveEnd - solveStart).count();

    if (solver.info() != Success)
        throw std::runtime_error("PARDISO solve failed!");

    VectorXd r = A * x - b;
    result.residual      = r.norm();
    result.backwardError = result.residual / (A.norm() * x.norm() + b.norm() + 1e-18);

    result.peakRssMB = peakRSS_MB();
    return result;
}