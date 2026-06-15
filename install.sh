#!/usr/bin/env bash
#
# install.sh — развёртывание стенда SparseBenchmark на чистой Ubuntu 24.04
#
# Версии зафиксированы под рабочую машину (Ubuntu 24.04.4, GCC 13.3.0):
#   Eigen 3.4.0 | OpenBLAS 0.3.26 | LAPACK 3.12 | OpenMPI 4.1.6
#   ScaLAPACK 2.2.1 | METIS 5.1.0 | MKL 2026.0 | MUMPS 5.8.0 | SuperLU_MT 4.0.2
#
# Использование:
#   ./install.sh            полная установка
#   ./install.sh apt        системные пакеты
#   ./install.sh mkl        Intel oneAPI MKL 2026.0
#   ./install.sh mumps      сборка MUMPS 5.8.0
#   ./install.sh superlu    сборка SuperLU_MT 4.0.2
#   ./install.sh build      сборка проекта
#   ./install.sh check      диагностика (ничего не ставит)
#   ./install.sh clean      удалить сборочный мусор (/tmp, build/)
#   ./install.sh clean-all  + удалить собранные MUMPS/SuperLU (с подтверждением)
#
set -uo pipefail

PROJECT_DIR="${PROJECT_DIR:-$HOME/SparseBenchmark}"
ONEAPI_ENV="/opt/intel/oneapi/setvars.sh"
NPROC="$(nproc)"

MUMPS_VER="5.8.0"
MUMPS_URL="https://coin-or-tools.github.io/ThirdParty-Mumps/MUMPS_${MUMPS_VER}.tar.gz"
SUPERLU_REPO="https://github.com/xiaoyeli/superlu_mt.git"
MKL_PKG="intel-oneapi-mkl-devel-2026.0"   # =2026.0.0-908

c_ok()   { printf '\033[32m[OK]\033[0m %s\n' "$*"; }
c_skip() { printf '\033[34m[--]\033[0m %s\n' "$*"; }
c_warn() { printf '\033[33m[!!]\033[0m %s\n' "$*"; }
c_err()  { printf '\033[31m[XX]\033[0m %s\n' "$*" >&2; }
c_step() { printf '\n\033[1;36m===== %s =====\033[0m\n' "$*"; }
die() { c_err "$*"; exit 1; }
have_pkg() { dpkg -l "$1" 2>/dev/null | grep -q '^ii'; }

step_apt() {
    c_step "Системные пакеты (apt)"
    local pkgs=(build-essential gfortran cmake git wget tar
        libeigen3-dev libopenblas-dev liblapack-dev
        libopenmpi-dev openmpi-bin libscalapack-openmpi-dev libmetis-dev)
    local missing=()
    for p in "${pkgs[@]}"; do have_pkg "$p" || missing+=("$p"); done
    if [ ${#missing[@]} -eq 0 ]; then c_skip "все пакеты установлены"; return 0; fi
    c_warn "ставлю: ${missing[*]}"
    sudo apt-get update || die "apt update"
    sudo apt-get install -y "${missing[@]}" || die "apt install"
    c_ok "системные пакеты установлены"
}

step_mkl() {
    c_step "Intel oneAPI MKL 2026.0"
    if [ -f "$ONEAPI_ENV" ] && [ -d /opt/intel/oneapi/mkl/2026.0 ]; then
        c_skip "MKL 2026.0 уже установлен"; return 0; fi
    local keyring="/usr/share/keyrings/oneapi-archive-keyring.gpg"
    if [ ! -f "$keyring" ]; then
        c_warn "ключ и репозиторий Intel"
        wget -O- https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
            | gpg --dearmor | sudo tee "$keyring" >/dev/null || die "ключ Intel (сеть?)"
        echo "deb [signed-by=$keyring] https://apt.repos.intel.com/oneapi all main" \
            | sudo tee /etc/apt/sources.list.d/oneAPI.list >/dev/null
    fi
    sudo apt-get update || die "apt update"
    if apt-cache show "$MKL_PKG" >/dev/null 2>&1; then
        sudo apt-get install -y "$MKL_PKG" || die "установка $MKL_PKG"
        c_ok "установлен $MKL_PKG"
    else
        c_warn "$MKL_PKG недоступен, ставлю последний intel-oneapi-mkl-devel"
        sudo apt-get install -y intel-oneapi-mkl-devel || die "установка MKL"
    fi
    [ -f "$ONEAPI_ENV" ] || die "setvars.sh не найден после установки MKL"
    c_ok "MKL установлен (source $ONEAPI_ENV для активации)"
}

step_mumps() {
    c_step "MUMPS $MUMPS_VER (сборка)"
    local dst="$PROJECT_DIR/mumps_local"
    if [ -f "$dst/lib/libdmumps.a" ]; then c_skip "MUMPS уже собран"; return 0; fi
    local src="/tmp/MUMPS_${MUMPS_VER}" tgz="/tmp/MUMPS_${MUMPS_VER}.tar.gz"
    if [ ! -d "$src" ]; then
        c_warn "скачиваю $MUMPS_URL"
        wget -O "$tgz" "$MUMPS_URL" || die "скачивание MUMPS (сеть/DNS?)"
        tar xzf "$tgz" -C /tmp || die "распаковка MUMPS"
    fi
    cd "$src" || die "нет $src"
    if [ ! -f Makefile.inc ]; then
        cp Make.inc/Makefile.debian.PAR ./Makefile.inc || die "нет шаблона debian.PAR"
        sed -i 's/^LIBBLAS = -lblas/LIBBLAS = -lopenblas/'                            Makefile.inc
        sed -i 's/^LSCOTCH   = -L.*scotcherr/LSCOTCH   =/'                            Makefile.inc
        sed -i 's|^ISCOTCH   = -I/usr/include/scotch|ISCOTCH   =|'                    Makefile.inc
        sed -i 's/^ORDERINGSF = -Dmetis -Dpord -Dscotch/ORDERINGSF = -Dmetis -Dpord/' Makefile.inc
        c_ok "Makefile.inc: OpenBLAS, metis+pord, без scotch"
    fi
    c_warn "make d -j$NPROC (несколько минут)"
    make d -j"$NPROC" || die "сборка MUMPS — см. вывод выше"
    [ -f lib/libdmumps.a ] || die "libdmumps.a не появилась"
    mkdir -p "$dst/lib" "$dst/include"
    cp lib/libdmumps.a lib/libmumps_common.a lib/libpord.a "$dst/lib/" || die "копирование .a"
    cp include/*.h "$dst/include/"
    c_ok "MUMPS установлен → $dst"
    cat > /tmp/_mabi.c <<'EOF'
#include <dmumps_c.h>
#include <stdio.h>
int main(){ printf("%zu\n", sizeof(DMUMPS_STRUC_C)); return 0; }
EOF
    if gcc /tmp/_mabi.c -I "$dst/include" -o /tmp/_mabi 2>/dev/null; then
        c_ok "sizeof(DMUMPS_STRUC_C) = $(/tmp/_mabi) байт (код выделяет sizeof+8192)"
    fi
}

step_superlu() {
    c_step "SuperLU_MT 4.0.2 (PTHREAD)"
    if [ -f /usr/local/include/superlu_mt/slu_mt_ddefs.h ] \
       && ls /usr/local/lib/libsuperlu_mt_PTHREAD.a >/dev/null 2>&1; then
        c_skip "SuperLU_MT уже установлен"; return 0; fi
    local src=/tmp/superlu_mt
    if [ ! -d "$src" ]; then
        c_warn "клонирую $SUPERLU_REPO"
        git clone "$SUPERLU_REPO" "$src" || die "git clone (сеть/DNS?)"
    fi
    cd "$src" || die "нет $src"
    mkdir -p lib    # каталог для .a; в репозитории его нет, ar без него падает
    cat > make.inc <<EOF
SuperLUroot = $src
PLAT       = _PTHREAD
TMGLIB     = libtmglib.a
SUPERLULIB = ../lib/libsuperlu_mt\$(PLAT).a
BLASDEF    = -DUSE_VENDOR_BLAS
BLASLIB    = -lopenblas
MATHLIB    = -lm
MPLIB      = -lpthread
CC         = gcc
CFLAGS     = -O3 -fopenmp -D__PTHREAD -DAdd_ -Wno-unused-result
LOADER     = gcc
LOADOPTS   = -O3 -fopenmp
FORTRAN    = gfortran
FFLAGS     = -O3
ARCH       = ar
ARCHFLAGS  = cr
RANLIB     = ranlib
EOF
    c_ok "make.inc: PTHREAD + OpenBLAS"

    # Верхний Makefile (цель lib) собирает все типы s/d/c/z и спотыкается на single.
    # Нам нужен только double — собираем его напрямую через SRC/Makefile.
    c_warn "make -C SRC double -j$NPROC (нужен только double precision)"
    make -C SRC double -j"$NPROC" || die "сборка SuperLU_MT — см. вывод выше"
    [ -f lib/libsuperlu_mt_PTHREAD.a ] \
        || die "libsuperlu_mt_PTHREAD.a не появилась — см. вывод сборки выше"
    local libfile; libfile=$(ls lib/libsuperlu_mt_PTHREAD.a 2>/dev/null)
    [ -n "$libfile" ] || die "libsuperlu_mt_PTHREAD.a не появилась"
    sudo mkdir -p /usr/local/include/superlu_mt /usr/local/lib
    sudo cp SRC/*.h /usr/local/include/superlu_mt/ || die "копирование заголовков"
    sudo cp "$libfile" /usr/local/lib/
    sudo ldconfig
    c_ok "SuperLU_MT установлен (libsuperlu_mt_PTHREAD.a)"
}

step_build() {
    c_step "Сборка проекта"
    [ -d "$PROJECT_DIR" ] || die "нет каталога проекта: $PROJECT_DIR"
    cd "$PROJECT_DIR" || die "cd $PROJECT_DIR"
    if [ -f "$ONEAPI_ENV" ]; then
        source "$ONEAPI_ENV" --quiet 2>/dev/null
        [ -n "${MKLROOT:-}" ] && c_ok "MKL активирован: $MKLROOT" \
                              || c_warn "MKLROOT пуст — PARDISO может не слинковаться"
    else
        c_warn "oneAPI не найден — solver_pardiso не соберётся (./install.sh mkl)"
    fi
    rm -rf build
    cmake -B build -S . || die "cmake configure"
    cmake --build build -j"$NPROC" || die "сборка проекта"
    local n=0
    for s in solver_mumps solver_pardiso solver_superlu; do
        [ -x "build/$s" ] && { c_ok "собран: $s"; n=$((n+1)); } || c_warn "НЕ собран: $s"
    done
    [ "$n" -gt 0 ] || die "ни один солвер не собрался"
}

# ════════════════════════════════════════════════════════════════════════════
# Очистка
# ════════════════════════════════════════════════════════════════════════════
step_clean() {
    c_step "Очистка сборочного мусора"
    rm -rf "/tmp/MUMPS_${MUMPS_VER}" "/tmp/MUMPS_${MUMPS_VER}.tar.gz" /tmp/superlu_mt \
           /tmp/_mabi.c /tmp/_mabi 2>/dev/null
    c_ok "удалены /tmp/MUMPS_${MUMPS_VER}, /tmp/superlu_mt и временные файлы"
    if [ -d "$PROJECT_DIR/build" ]; then
        rm -rf "$PROJECT_DIR/build"
        c_ok "удалён $PROJECT_DIR/build"
    fi
    c_warn "установленные библиотеки НЕ тронуты (полный сброс: ./install.sh clean-all)"
}

step_clean_all() {
    c_step "Полная очистка (включая установленные библиотеки)"
    c_warn "Будут удалены: собранный MUMPS, SuperLU_MT из /usr/local, сборка проекта."
    c_warn "MKL и системные apt-пакеты НЕ затрагиваются."
    printf "Продолжить? Введите yes для подтверждения: "
    read -r ans
    [ "$ans" = "yes" ] || { c_skip "отменено"; return 0; }
    step_clean
    if [ -d "$PROJECT_DIR/mumps_local" ]; then
        rm -rf "$PROJECT_DIR/mumps_local"
        c_ok "удалён $PROJECT_DIR/mumps_local"
    fi
    sudo rm -f /usr/local/lib/libsuperlu_mt_PTHREAD.a 2>/dev/null \
        && c_ok "удалён /usr/local/lib/libsuperlu_mt_PTHREAD.a"
    sudo rm -rf /usr/local/include/superlu_mt 2>/dev/null \
        && c_ok "удалён /usr/local/include/superlu_mt"
    sudo ldconfig
    c_warn "MKL оставлен. Для удаления: sudo apt remove '$MKL_PKG' (вручную)"
    c_ok "полная очистка завершена — можно ставить заново через ./install.sh"
}

step_check() {
    c_step "Диагностика окружения"
    chk(){ printf '%-34s' "$1"; shift; "$@" && c_ok "есть" || c_err "нет"; }
    printf '%-34s%s\n' "ОС:"  "$(. /etc/os-release; echo "$PRETTY_NAME")"
    printf '%-34s%s\n' "GCC:" "$(gcc -dumpversion 2>/dev/null)"
    chk "Eigen3:"     have_pkg libeigen3-dev
    chk "OpenBLAS:"   have_pkg libopenblas-dev
    chk "LAPACK:"     have_pkg liblapack-dev
    chk "OpenMPI:"    have_pkg libopenmpi-dev
    chk "ScaLAPACK:"  have_pkg libscalapack-openmpi-dev
    chk "METIS:"      have_pkg libmetis-dev
    printf '%-34s' "Intel MKL 2026.0:"; [ -d /opt/intel/oneapi/mkl/2026.0 ] && c_ok "есть" || c_err "нет"
    printf '%-34s' "MUMPS (собран):";   [ -f "$PROJECT_DIR/mumps_local/lib/libdmumps.a" ] && c_ok "есть" || c_err "нет"
    printf '%-34s' "SuperLU_MT (собран):"; [ -f /usr/local/lib/libsuperlu_mt_PTHREAD.a ] && c_ok "есть" || c_err "нет"
    printf '%-34s' "бинарники проекта:"; ls "$PROJECT_DIR"/build/solver_* >/dev/null 2>&1 && c_ok "есть" || c_warn "нет"
}

main() {
    case "${1:-all}" in
        apt) step_apt ;; mkl) step_mkl ;; mumps) step_mumps ;;
        superlu) step_superlu ;; build) step_build ;; check) step_check ;;
        clean) step_clean ;; clean-all) step_clean_all ;;
        all)
            step_apt; step_mkl; step_mumps; step_superlu; step_build
            c_step "Готово"; step_check
            printf '\nЗапуск: ./run_all.sh matrices/<матрица>.mtx 4\n' ;;
        *) die "шаг: apt|mkl|mumps|superlu|build|check|clean|clean-all|all" ;;
    esac
}
main "$@"