#!/bin/bash
SOLVER="$1"
MATRIX="$2"
REPEATS="${3:-1}"   # 3-й аргумент — число повторов, по умолчанию 1

if [[ -z "$SOLVER" || -z "$MATRIX" ]]; then
    echo "Использование: $0 <superlu|pardiso|mumps> <путь/к/матрице.mtx> [повторы]"
    echo "Пример: $0 mumps matrices/conv3d_50.mtx 3"
    exit 1
fi

[ -z "$SETVARS_COMPLETED" ] && source /opt/intel/oneapi/setvars.sh --quiet

# CSV-файл: results/<солвер>_<матрица>_<дата-время>.csv
mkdir -p results
STAMP=$(date +%Y%m%d_%H%M%S)
MNAME=$(basename "$MATRIX" .mtx)
CSV="results/${SOLVER}_${MNAME}_${STAMP}.csv"

# Заголовок CSV
echo "solver,matrix,rows,nnz,threads,run,factor_s,solve_s,residual,bwderr,mem_fact_mb,mem_rss_mb" > "$CSV"

echo "====== Солвер: $SOLVER | Матрица: $(basename $MATRIX) | Повторов: $REPEATS ======"
echo "CSV → $CSV"

run_solver() {
    local threads=$1 run=$2
    case "$SOLVER" in
        pardiso)
            MKL_THREADING_LAYER=intel MKL_NUM_THREADS=$threads OMP_NUM_THREADS=$threads \
            ./build/solver_pardiso "$MATRIX" --threads "$threads" --run "$run" ;;
        mumps)
            OMP_NUM_THREADS=$threads \
            mpirun -n 1 ./build/solver_mumps "$MATRIX" --threads "$threads" --run "$run" ;;
        superlu)
            ./build/solver_superlu "$MATRIX" --threads "$threads" --run "$run" ;;
        *)
            echo "Неизвестный солвер: $SOLVER" >&2; exit 1 ;;
    esac
}

for threads in 1 2 4; do
    echo ""
    echo "--- $threads поток(а) ---"

    for ((run=1; run<=REPEATS; run++)); do
        [ "$REPEATS" -gt 1 ] && echo "  [прогон $run/$REPEATS]"

        # Захватываем stdout; stderr (ошибки) идёт на терминал живьём
        OUT=$(run_solver "$threads" "$run")

        # Человеку — всё, кроме служебной CSV-строки
        echo "$OUT" | grep -v '^CSV,'
        # В файл — CSV-строка без префикса
        echo "$OUT" | grep '^CSV,' | sed 's/^CSV,//' >> "$CSV"
    done
done

echo ""
echo "Готово. Результаты: $CSV"

# Красивая таблица в конце, если есть column
if command -v column >/dev/null 2>&1; then
    echo ""
    column -t -s, "$CSV"
fi