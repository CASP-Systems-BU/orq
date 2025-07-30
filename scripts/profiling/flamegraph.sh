#!/usr/bin/env bash
#
# Run a given executable and collect the flamegraph.
# Usage:
#  ./flamegraph.sh [executable] [other args]
# 
# executable : which binary to compile and run
# other args : additional command line arguments to pass to `executable`
#
# To configure threading, protocol, and batch size, use the variables below
# for the time being.

set -e

NUM_PARTIES=3
THREADS="8"
BATCH="-12"

# Confirm only executable provided
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 [exec]  ..."
    exit 1
fi

EXEC=$1
EXTRA_ARGS="${@:2}"

if [ ! -d "$HOME/flamegraph" ]; then
    echo "== Cloning Flamegraph from Github =="
    git clone https://github.com/brendangregg/Flamegraph.git "$HOME/flamegraph"

    echo "== Installing perf =="
    sudo apt -q update
    # NOTE: This will need to updated as kernels get updated
    sudo apt -qy install linux-tools-common linux-tools-generic linux-tools-5.15.0-131-generic
fi

echo "== System Setup =="
sudo mount -o remount,mode=755 /sys/kernel/tracing/
sudo chmod -R o+rx /sys/kernel/tracing
sudo sysctl kernel.perf_event_paranoid=-1
echo 0 | sudo tee /proc/sys/kernel/kptr_restrict

echo "== Compiling ==" 
cmake .. -DPROTOCOL=$NUM_PARTIES -DPROFILE=1
make -j $EXEC

EXEC_STRING="./$EXEC $THREADS 1 $BATCH $EXTRA_ARGS"

echo "== Running =="

HOST_LIST=""
for ((i=0; i<=NUM_PARTIES-1; i++)); do
    HOST_LIST+="node$i "
    if [ $i -eq 0 ]; then
       continue
    fi
    scp $EXEC node$i:$(pwd)
done

HOST_LIST=$(echo $HOST_LIST | sed 's/ $//' | tr ' ' ',')  # Remove trailing space

echo "  Using remote: $HOST_LIST"

# Run 1 instance under perf, locally and the rest normally on remote machines
# If you want to profile someone besides party 0 (or use fully-local execution),
# this needs to be modified.
mpirun --host $HOST_LIST -n 1 perf record -F 100 -g -- $EXEC_STRING : \
    -n $((NUM_PARTIES - 1)) $EXEC_STRING

echo "== Generating Flamegraph =="
perf script | "$HOME/flamegraph/stackcollapse-perf.pl" | \
    "$HOME/flamegraph/flamegraph.pl" > "$EXEC.svg"
