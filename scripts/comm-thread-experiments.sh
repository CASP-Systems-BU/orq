#!/usr/bin/env bash

trap "exit 1" SIGINT

BATCH_SIZE=65536

cd $(dirname $0)
mkdir -p ../build
cd ../build

folder="../results/communicator-benchmark/thread-profiling/$(date +%m%d-%H%M)"
mkdir -p $folder
mkdir -p $folder/raw_data

# Clean up!
rm thread-*-out.csv

# Get profiling ready
sudo mount -o remount,mode=755 /sys/kernel/tracing/
sudo chmod -R o+rx /sys/kernel/tracing
sudo sysctl kernel.perf_event_paranoid=-1
echo 0 | sudo tee /proc/sys/kernel/kptr_restrict

set -e

run_threading_test() {
    local ENVIRO=$1
    local PROTOCOL=$2
    local COMMUNICATOR=$3
    local COMMAND=$4

    cmake .. -DPROTOCOL=$PROTOCOL -DPERF=1 -DCOMM=$COMMUNICATOR
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
        for OP in AND EQ GR RCA; do
            TEST_NAME=${COMMUNICATOR}-${ENVIRO}-${PROTOCOL}pc-${OP}-${T}thr
            echo "== Running $TEST_NAME @ $(date +%R) | $(eval echo $COMMAND)"
            
            eval $COMMAND

            output_file="$folder/raw_data/$TEST_NAME.threads"
            result_file="$folder/raw_data/$TEST_NAME.txt"
            
            mv thread-*-out.csv $output_file

            python3 ../scripts/plot-threading.py $output_file > $result_file

            rm $output_file
            
            echo "== Finished $TEST_NAME @ $(date +%R)"
        done
    done

    echo
}

EXEC="./thread_test \$T 1 $BATCH_SIZE \$OP"

run_threading_test lan 3 MPI "mpirun -np 3 --host node0,node1,node2 $EXEC"

run_threading_test lan 3 NOCOPY "startmpc -n 3 -h node0,node1,node2 $EXEC"
