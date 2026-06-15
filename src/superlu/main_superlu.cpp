#include <iostream>
#include <string>
#include "superlu_solver.h"
#include "../common/matrix_loader.h"

// Использование: ./solver_superlu <matrix.mtx> [--threads N] [--run K]
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0]
                  << " <matrix.mtx> [--threads N] [--run K]\n";
        return 1;
    }

    std::string matrixPath = argv[1];
    int threads = 1;
    int runIdx  = 1;

    for (int i = 2; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "--threads") threads = std::stoi(argv[i + 1]);
        if (std::string(argv[i]) == "--run")     runIdx  = std::stoi(argv[i + 1]);
    }

    Eigen::SparseMatrix<double> A;
    Eigen::VectorXd b;
    if (!loadMatrix(matrixPath, A, b)) return 1;

    BenchmarkConfig cfg;
    cfg.threads = threads;

    std::cout << "\n[SuperLU] потоков: " << threads << "\n";
    try {
        auto res = runSuperLU(A, b, cfg);
        res.matrixName = extractMatrixName(matrixPath);

        std::cout << "  Factor : " << res.factorTime    << " с\n"
                  << "  Solve  : " << res.solveTime     << " с\n"
                  << "  Resid  : " << res.residual      << "\n"
                  << "  BwdErr : " << res.backwardError << "\n"
                  << "  MemFact: " << res.solverMemMB   << " MB\n"
                  << "  MemRSS : " << res.peakRssMB     << " MB\n";

        // Машинно-читаемая строка для CSV (префикс CSV, его ловит run_bench.sh)
        std::cout << "CSV,"
                  << res.solver        << ","
                  << res.matrixName    << ","
                  << res.rows          << ","
                  << res.nnz           << ","
                  << res.threads       << ","
                  << runIdx            << ","
                  << res.factorTime    << ","
                  << res.solveTime     << ","
                  << res.residual      << ","
                  << res.backwardError << ","
                  << res.solverMemMB   << ","
                  << res.peakRssMB     << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }

    return 0;
}