#include "mumps_solver.h"
#include <chrono>
#include <vector>
#include <stdexcept>
#include <omp.h>
#include <mpi.h>
#include "mem_util.h"
#include <Eigen/Dense>
#include <Eigen/Sparse>

extern "C" {
    #include <dmumps_c.h>
}

#define JOB_INIT -1
#define JOB_END  -2

using namespace Eigen;

SolverResult runMUMPS(
    const SparseMatrix<double>& A,
    const VectorXd& b,
    const BenchmarkConfig& cfg)
{
    SolverResult result;
    result.solver  = "MUMPS";
    result.threads = cfg.threads;
    result.rows    = A.rows();
    result.cols    = A.cols();
    result.nnz     = A.nonZeros();

    omp_set_num_threads(cfg.threads);

    SparseMatrix<double> Acopy = A;
    Acopy.makeCompressed();

    std::vector<MUMPS_INT> irn(Acopy.nonZeros());
    std::vector<MUMPS_INT> jcn(Acopy.nonZeros());
    std::vector<double>    aval(Acopy.nonZeros());

    int k = 0;
    for (int col = 0; col < Acopy.outerSize(); ++col) {
        for (SparseMatrix<double>::InnerIterator it(Acopy, col); it; ++it) {
            irn[k]  = static_cast<MUMPS_INT>(it.row() + 1);
            jcn[k]  = static_cast<MUMPS_INT>(it.col() + 1);
            aval[k] = it.value();
            ++k;
        }
    }

    std::vector<double> rhs_buf(b.data(), b.data() + b.size());

   
    std::vector<char> id_mem(sizeof(DMUMPS_STRUC_C) + 8192, 0);
    DMUMPS_STRUC_C& id = *reinterpret_cast<DMUMPS_STRUC_C*>(id_mem.data());

    id.job          = JOB_INIT;
    id.par          = 1;
    id.sym          = 0;
    id.comm_fortran = (MUMPS_INT) MPI_Comm_c2f(MPI_COMM_WORLD);
    dmumps_c(&id);

    id.n   = static_cast<MUMPS_INT>(Acopy.rows());
    id.nz  = static_cast<MUMPS_INT>(k);
    id.irn = irn.data();
    id.jcn = jcn.data();
    id.a   = aval.data();

    id.nrhs = 1;
    id.lrhs = id.n;
    id.rhs  = rhs_buf.data();

    id.icntl[0] = -1;
    id.icntl[1] = -1;
    id.icntl[2] = -1;
    id.icntl[3] =  0;

    // Analysis
    id.job = 1;
    // std::cout << "  Ordering (INFOG7): " << id.infog[6] << "\n";
    dmumps_c(&id);
    if (id.info[0] < 0)
        throw std::runtime_error("MUMPS analysis failed: " + std::to_string(id.info[0]));

    // Factorization
    auto factorStart = std::chrono::high_resolution_clock::now();
    id.job = 2;
    dmumps_c(&id);
    auto factorEnd = std::chrono::high_resolution_clock::now();
    result.factorTime = std::chrono::duration<double>(factorEnd - factorStart).count();

    if (id.info[0] < 0)
        throw std::runtime_error("MUMPS factorization failed: " + std::to_string(id.info[0]));

    // Solve
    auto solveStart = std::chrono::high_resolution_clock::now();
    id.job = 3;
    dmumps_c(&id);
    auto solveEnd = std::chrono::high_resolution_clock::now();
    result.solveTime = std::chrono::duration<double>(solveEnd - solveStart).count();

    if (id.info[0] < 0)
        throw std::runtime_error("MUMPS solve failed: " + std::to_string(id.info[0]));


    if (id.info[0] < 0)
        throw std::runtime_error("MUMPS factorization failed: " + std::to_string(id.info[0]));

    // INFOG(22)=infog[21] — пик памяти на процесс при факторизации, уже в MB
    result.solverMemMB = static_cast<double>(id.infog[21]);

    VectorXd x = Map<VectorXd>(rhs_buf.data(), rhs_buf.size());

    VectorXd r = A * x - b;
    result.residual      = r.norm();
    result.backwardError = result.residual / (A.norm() * x.norm() + b.norm() + 1e-18);

    id.job = JOB_END;
    dmumps_c(&id);

    result.peakRssMB = peakRSS_MB();
    return result;
}