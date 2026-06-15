#pragma once
#include <Eigen/Sparse>
#include <string>

// Загружает матрицу из .mtx файла.
// Если матрица прямоугольная — автоматически строит A^T * A.
// Возвращает false при ошибке загрузки.
bool loadMatrix(const std::string& path,
                Eigen::SparseMatrix<double>& A_out,
                Eigen::VectorXd& b_out);

// Извлекает имя файла из полного пути
std::string extractMatrixName(const std::string& path);