#!/usr/bin/env bash

trap "exit 1" SIGINT

usage() {
    echo "Usage: $0 [batch|thread]"
    exit 1
}

(( $# < 1 )) && usage

# Choose experiment to run
if [[ $1 == "batch" ]]; then
    EXP_NAME="micro_comm_batch"

    INPUT_SIZES=10,11,12,13,14,15,16,17,18,19,20
    THREADS="0"
    folder_label="batch-latency"

elif [[ $1 == "thread" ]]; then
    EXP_NAME="micro_comm_threads"

    INPUT_SIZES=16
    THREADS="0-6"
    folder_label="thread-latency"

else
    usage
fi

# Script parameters
ENVIRO=lan
BITWIDTH=32

CMAKE="-DDEFAULT_BITWIDTH=$BITWIDTH"

cd $(dirname $0)

folder="../results/communicator-benchmark/$folder_label/$(date +%m%d-%H%M)"
mkdir -p $folder
mkdir -p $folder/raw_data

[[ $ENVIRO == "wan" ]] && ./cluster-wan-sim.sh on node{1,2}

cat <<EOF > $folder/meta.txt
Timestamp: $(date)

Git commit: $(git rev-parse HEAD)

Git status: $(git status)

Command line: $0 $*

Args:
ENVIRO=$ENVIRO
INPUT_SIZES=$INPUT_SIZES

Hostnames: $(hostname); $(hostname -A)

CPU: ($(hostname))
$(lscpu)

Network status:
$(ip a)

Simulator status:
$(tc qdisc)

EOF

# MPI - Baseline
./run_experiment.sh -p 3 -s $ENVIRO -c mpi -m "$CMAKE" -r $INPUT_SIZES -t $THREADS -x node $EXP_NAME 2>&1 | tee "$folder/raw_data/${ENVIRO}-mpi.txt"

# SocketComm
# ./run_experiment.sh -p 3 -s $ENVIRO -c socket -m "$CMAKE" -r $INPUT_SIZES  -t $THREADS -x node $EXP_NAME 2>&1 | tee "$folder/raw_data/${ENVIRO}-socket.txt"

# NoCopyComm (N comm threads)
./run_experiment.sh -p 3 -s $ENVIRO -c nocopy -m "$CMAKE" -r $INPUT_SIZES -t $THREADS -x node $EXP_NAME 2>&1 | tee "$folder/raw_data/${ENVIRO}-nocopy.txt"

if [[ $EXP_NAME == "micro_comm_threads" ]]; then
    for i in 1 2 4; do  # Number of comm threads
        ./run_experiment.sh -p 3 -s $ENVIRO -c nocopy -n $i -m "$CMAKE" -r $INPUT_SIZES -t $THREADS -x node $EXP_NAME 2>&1 | tee "$folder/raw_data/${ENVIRO}-nocopy-$i.txt"
    done
fi

[[ $ENVIRO == "wan" ]] && ./cluster-wan-sim.sh off node{1,2}
