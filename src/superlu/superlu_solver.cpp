#include "superlu_solver.h"
#include "mem_util.h"
#include <chrono>
#include <vector>
#include <stdexcept>
#include <iostream>

// SuperLU_MT (новая версия 4.x с GitHub): API через superlumt_options_t / Gstat_t.
// НЕ подключать Eigen/SuperLUSupport и <mkl.h> — конфликтуют с заголовками MT.
extern "C" {
    #include <slu_mt_ddefs.h>
}

using namespace Eigen;

SolverResult runSuperLU(
    const SparseMatrix<double>& A,
    const VectorXd& b,
    const BenchmarkConfig& cfg)
{
    SolverResult result;
    result.solver  = "SuperLU_MT";
    result.threads = cfg.threads;
    result.rows    = A.rows();
    result.cols    = A.cols();
    result.nnz     = A.nonZeros();

    SparseMatrix<double, ColMajor> Acsc = A;
    Acsc.makeCompressed();

    const int_t n   = static_cast<int_t>(Acsc.rows());
    const int_t nnz = static_cast<int_t>(Acsc.nonZeros());

    // Копии данных — факторизация модифицирует их
    std::vector<double> vals  (Acsc.valuePtr(),       Acsc.valuePtr()      + nnz);
    std::vector<int_t>  rowind(Acsc.innerIndexPtr(),  Acsc.innerIndexPtr() + nnz);
    std::vector<int_t>  colptr(Acsc.outerIndexPtr(),  Acsc.outerIndexPtr() + n + 1);
    std::vector<double> rhs   (b.data(),              b.data()             + n);
    std::vector<int_t>  perm_c(n, 0), perm_r(n, 0);

    SuperMatrix sA, sAC, sL, sU, sB;

    dCreate_CompCol_Matrix(&sA, n, n, nnz,
                           vals.data(), rowind.data(), colptr.data(),
                           SLU_NC, SLU_D, SLU_GE);
    dCreate_Dense_Matrix(&sB, n, 1, rhs.data(), n,
                         SLU_DN, SLU_D, SLU_GE);

    // Параметры
    const int_t  nprocs            = cfg.threads;   // параллелизм
    const int_t  panel_size        = sp_ienv(1);
    const int_t  relax             = sp_ienv(2);
    const double diag_pivot_thresh = 1.0;
    const double drop_tol          = 0.0;

    // Статистика — нужна и для factor, и для solve
    Gstat_t Gstat;
    StatAlloc(n, nprocs, panel_size, relax, &Gstat);
    StatInit(n, nprocs, &Gstat);

    // ── Предобработка (как analysis у MUMPS — не замеряем) ──────────────────
    // Перестановка столбцов: 0=natural, 1=MMD(A'A), 2=MMD(A'+A), 3=COLAMD
    get_perm_c(3, &sA, perm_c.data());

    superlumt_options_t opts;
    // pdgstrf_init заполняет opts, выделяет etree/colcnt внутри и строит sAC
    pdgstrf_init(nprocs, DOFACT, NOTRANS, NO,
                 panel_size, relax,
                 diag_pivot_thresh, NO /*usepr*/, drop_tol,
                 perm_c.data(), perm_r.data(),
                 nullptr /*work*/, 0 /*lwork*/,
                 &sA, &sAC, &opts, &Gstat);

    // ── Факторизация (ПАРАЛЛЕЛЬНАЯ: nprocs потоков PTHREAD) ─────────────────
    int_t info = 0;
    auto factorStart = std::chrono::high_resolution_clock::now();
    pdgstrf(&opts, &sAC, perm_r.data(), &sL, &sU, &Gstat, &info);
    auto factorEnd   = std::chrono::high_resolution_clock::now();
    result.factorTime = std::chrono::duration<double>(factorEnd - factorStart).count();

    if (info != 0) {
        std::cerr << "  [ERROR SuperLU_MT] pdgstrf failed: info=" << info << "\n";
        result.solveTime     = 0.0;
        result.residual      = -1.0;
        result.backwardError = -1.0;
        pxgstrf_finalize(&opts, &sAC);
        StatFree(&Gstat);
        Destroy_SuperMatrix_Store(&sA);
        Destroy_SuperMatrix_Store(&sB);
        return result;
    }

    // Память факторов: total_needed в байтах. Читать ДО Destroy_*/finalize.
    superlu_memusage_t mem;
    superlu_dQuerySpace(nprocs, &sL, &sU, panel_size, &mem);
    result.solverMemMB = mem.total_needed / (1024.0 * 1024.0);

    // ── Solve (ОДНОПОТОЧНЫЙ — архитектурное ограничение SuperLU_MT) ─────────
    auto solveStart = std::chrono::high_resolution_clock::now();
    dgstrs(NOTRANS, &sL, &sU, perm_r.data(), perm_c.data(), &sB, &Gstat, &info);
    auto solveEnd   = std::chrono::high_resolution_clock::now();
    result.solveTime = std::chrono::duration<double>(solveEnd - solveStart).count();

    if (info != 0) {
        std::cerr << "  [ERROR SuperLU_MT] dgstrs failed: info=" << info << "\n";
        result.residual      = -1.0;
        result.backwardError = -1.0;
    } else {
        double* xptr = static_cast<double*>(
            static_cast<DNformat*>(sB.Store)->nzval);
        Map<const VectorXd> x(xptr, n);
        VectorXd r = A * x - b;
        result.residual      = r.norm();
        result.backwardError = result.residual / (A.norm() * x.norm() + b.norm() + 1e-18);
    }

    // ── Cleanup ──────────────────────────────────────────────────────────────
    pxgstrf_finalize(&opts, &sAC);        // освобождает etree/colcnt и AC-store
    StatFree(&Gstat);
    Destroy_SuperMatrix_Store(&sA);       // только обёртка, данные в наших vectors
    Destroy_SuperMatrix_Store(&sB);
    Destroy_SuperNode_Matrix(&sL);        // факторы выделены внутри pdgstrf — освобождаем
    Destroy_CompCol_Matrix(&sU);

    result.peakRssMB = peakRSS_MB();
    return result;
}