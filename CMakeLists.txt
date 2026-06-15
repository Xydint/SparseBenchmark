cmake_minimum_required(VERSION 3.15)
project(SparseBenchmark CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------- опции сборки ----------
option(BUILD_SUPERLU  "Собрать solver_superlu"  ON)
option(BUILD_PARDISO  "Собрать solver_pardiso"  ON)
option(BUILD_MUMPS    "Собрать solver_mumps"    ON)

# ---------- зависимости ----------
find_package(Eigen3 REQUIRED NO_MODULE)
find_package(OpenMP REQUIRED)
find_package(MPI    REQUIRED)

# ---------- общая библиотека ----------
add_library(solver_common STATIC src/common/matrix_loader.cpp)
target_include_directories(solver_common PUBLIC src/common)
target_link_libraries(solver_common PUBLIC Eigen3::Eigen)


# ---------- SuperLU ----------
if(BUILD_SUPERLU)
    find_path(SUPERLU_MT_INCLUDE_DIR NAMES slu_mt_ddefs.h
        PATHS /usr/local/include/superlu_mt REQUIRED)
    find_library(SUPERLU_MT_LIB
        NAMES superlu_mt_PTHREAD superlu_mt_OPENMP superlu_mt
        PATHS /usr/local/lib REQUIRED)
    add_executable(solver_superlu
        src/superlu/main_superlu.cpp
        src/superlu/superlu_solver.cpp)
    target_include_directories(solver_superlu PRIVATE
        src/superlu ${SUPERLU_MT_INCLUDE_DIR})
    
    target_link_libraries(solver_superlu PRIVATE
        solver_common ${SUPERLU_MT_LIB} blas pthread)
endif()

# ---------- PARDISO (через MKL) ----------
if(BUILD_PARDISO)
    add_executable(solver_pardiso          
        src/pardiso/main_pardiso.cpp
        src/pardiso/pardiso_solver.cpp
    )
    target_compile_definitions(solver_pardiso PRIVATE EIGEN_USE_MKL_ALL)  
    target_include_directories(solver_pardiso PRIVATE src/pardiso)
    target_link_libraries(solver_pardiso PRIVATE
        solver_common
        OpenMP::OpenMP_CXX
        mkl_rt
    )
endif()

# проверить параллельную mkl в pardiso

# ---------- MUMPS ----------
if(BUILD_MUMPS)
    set(MUMPS_LOCAL ${CMAKE_SOURCE_DIR}/mumps_local)

    add_executable(solver_mumps
        src/mumps/main_mumps.cpp
        src/mumps/mumps_solver.cpp
    )
    target_include_directories(solver_mumps PRIVATE
        src/mumps
        ${MUMPS_LOCAL}/include
    )
    target_link_libraries(solver_mumps PRIVATE
        solver_common
        OpenMP::OpenMP_CXX
        MPI::MPI_CXX
        -L${MUMPS_LOCAL}/lib
        dmumps mumps_common pord
        scalapack-openmpi lapack openblas metis
        mpi_mpifh gfortran pthread
    )
endif()



#подготовить все что нужно установить; что можно установить на ubuntu