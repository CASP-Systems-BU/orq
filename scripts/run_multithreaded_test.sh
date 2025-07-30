#!/bin/bash
# Usage example: ../scripts/run_multithreaded_test.sh [PROTO] [THREADS]
# These default values can be overriden on the CLI
PROTOCOL=3
THREADS_NUM=2

if [[ -n "$1" ]]; then
  PROTOCOL="$1"
fi

if [[ -n "$2" ]]; then
  THREADS_NUM="$2"
fi

echo "[[ Running ${PROTOCOL}PC tests with $THREADS_NUM threads. ]]"

set -e

cd $(dirname $0)/../
mkdir -p build
cd build
make clean

COMMS=("MPI" "NOCOPY")
EXECS=(mpirun startmpc)

for i in "${!COMMS[@]}"; do
    echo "== Starting ${COMMS[$i]} tests"
    cmake .. -Wno-dev -DPROTOCOL=$PROTOCOL -DCOMM=${COMMS[$i]} -DEXTRA= -DCOMM_THREADS=1
    make -j tests-only
    for test in test_*
    do
        echo "== Running test:" $test
        ${EXECS[$i]} -n $PROTOCOL ./$test $THREADS_NUM || exit 1;
        echo "--------------------------------------"
    done
    echo "== All ${COMMS[$i]} tests passed!"
done

echo "[[ All ${PROTOCOL}PC $THREADS_NUM thread $COMMS tests passed! ]]"