#include "matrix_loader.h"
#include <unsupported/Eigen/SparseExtra>
#include <chrono>
#include <iostream>

using namespace Eigen;

std::string extractMatrixName(const std::string& path) {
    return path.substr(path.find_last_of("/\\") + 1);
}

bool loadMatrix(const std::string& path,
                SparseMatrix<double>& A_out,
                VectorXd& b_out)
{
    SparseMatrix<double> A;
    std::cout << "Загрузка матрицы: " << path << "...\n";

    if (!loadMarket(A, path)) {
        std::cerr << "Ошибка: не удалось загрузить матрицу!\n";
        return false;
    }

    std::cout << "Матрица: " << A.rows() << "x" << A.cols()
              << "  NNZ: " << A.nonZeros() << "\n";

    VectorXd b = VectorXd::Ones(A.rows());

    if (A.rows() != A.cols()) {
        std::cout << "[INFO] Прямоугольная матрица — строим A^T * A\n";
        auto t0 = std::chrono::high_resolution_clock::now();

        A_out = (A.transpose() * A).eval();
        b_out = A.transpose() * b;

        double dt = std::chrono::duration<double>(
            std::chrono::high_resolution_clock::now() - t0).count();
        std::cout << "[INFO] Трансформация: " << dt << " с\n";
    } else {
        A_out = A;
        b_out = b;
    }

    return true;
}