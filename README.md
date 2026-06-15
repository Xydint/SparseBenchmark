**_run_bench.sh_**
Использование: ./run_bench.sh <superlu|pardiso|mumps> <путь/к/матрице.mtx> [повторы]

**_install.sh_**

chmod +x install.sh
 Версии зафиксированы под рабочую машину (Ubuntu 24.04.4, GCC 13.3.0):
   Eigen 3.4.0 | OpenBLAS 0.3.26 | LAPACK 3.12 | OpenMPI 4.1.6
   ScaLAPACK 2.2.1 | METIS 5.1.0 | MKL 2026.0 | MUMPS 5.8.0 | SuperLU_MT 4.0.2

# Использование:
   ./install.sh            полная установка
   ./install.sh apt        системные пакеты
   ./install.sh mkl        Intel oneAPI MKL 2026.0
   ./install.sh mumps      сборка MUMPS 5.8.0
   ./install.sh superlu    сборка SuperLU_MT 4.0.2
   ./install.sh build      сборка проекта
   ./install.sh check      диагностика (ничего не ставит)
