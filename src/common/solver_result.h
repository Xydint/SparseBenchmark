#pragma once
#include <string>

struct SolverResult {
    std::string matrixName;
    std::string solver;
    int         threads    = 0; // кол-во потоков
    int         rows       = 0; // кол-во строк
    int         cols       = 0; // кол-во колоноко
    int         nnz        = 0; // NNZ
    double      factorTime = 0.0; // время факторизации матрицы
    double      solveTime  = 0.0; // время решения матрицы
    double      residual   = 0.0; // невязка
    double      backwardError = 0.0; // обратная ошибка
    double peakRssMB   = 0.0;   // пик памяти всего процесса (сравнимо между солверами)
    double solverMemMB = 0.0;   // память под факторы по статистике солвера
};