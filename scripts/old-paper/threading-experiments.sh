#!/usr/bin/env bash
# Run from build folder!

BATCH_SIZE=8192

trap "exit 1" SIGINT

sudo apt install -qq -y linux-tools-common linux-tools-$(uname -r) linux-tools-generic

# Clean up!
rm thread-*-out.csv
rm perf.data

mkdir -p threading-results

# Get profiling ready
sudo mount -o remount,mode=755 /sys/kernel/tracing/
sudo chmod -R o+rx /sys/kernel/tracing
sudo sysctl kernel.perf_event_paranoid=-1
echo 0 | sudo tee /proc/sys/kernel/kptr_restrict

set -e

# ALL_OPS="AND EQ GR RCA"
ALL_OPS="RS QS"

run_threading_test() {
    local ENVIRO=$1
    local PROTOCOL=$2
    local COMMAND=$3

    cmake .. -DPROTOCOL=$PROTOCOL -DPERF=1 -DEXTRA=-DINSTRUMENT_THREADS
    make thread_test

    # might as well
    scp thread_test node1:~/orq/build/ &
    scp thread_test node2:~/orq/build/ &
    wait

    echo "================================="
    echo "==== Start Test: ${PROTOCOL}pc $ENVIRO"
    echo "==== Command: $COMMAND"
    echo "================================="

    for T in 1 2 4 8; do
        for OP in $ALL_OPS; do
            TEST_NAME=${ENVIRO}-${PROTOCOL}pc-${OP}-${T}thr
            echo "== Running $TEST_NAME @ $(date +%R)"
            
            eval $COMMAND
            perf script > threading-results/$TEST_NAME.perf
            rm perf.data
            
            mv thread-*-out.csv threading-results/$TEST_NAME.threads
            
            echo "== Finished $TEST_NAME @ $(date +%R)"
        done
    done

    echo
}

PERF_BASE="perf record -F 100 -g --"
EXEC="./thread_test \$T 1 $BATCH_SIZE \$OP"

# run_threading_test local 1 "mpirun -n 1 $PERF_BASE $EXEC"
# run_threading_test local 2 "mpirun -n 1 $PERF_BASE $EXEC : -n 1 $EXEC"
run_threading_test local 3 "mpirun -n 1 $PERF_BASE $EXEC : -n 2 $EXEC"
# run_threading_test local 4 "mpirun -n 1 $PERF_BASE $EXEC : -n 3 $EXEC"

# run_threading_test lan 2 "mpirun -H node0 -n 1 $PERF_BASE $EXEC : -H node1 -n 1 $EXEC"
run_threading_test lan 3 "mpirun -H node0 -n 1 $PERF_BASE $EXEC : -H node1,node2 -n 2 $EXEC"
# run_threading_test lan 4 "mpirun -H node0 -n 1 $PERF_BASE $EXEC : -H node1,node2,node3 -n 3 $EXEC"

