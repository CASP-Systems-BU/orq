#!/usr/bin/env bash

trap "exit 1" SIGINT

usage() {
    echo "Usage: $0 <tpch|other|secretflow> <sf> <protocol> <threads> <enviro=lan> <query_range=1..22>"
    exit 1
}

(( $# < 3 )) && usage

cd $(dirname $0)

EXP_TYPE=$1

SCALE_FACTOR=$2
PROTOCOL=$3
THREADS=$4

# Default to LAN.
ENVIRO=${5:-lan}
# Default to all 22 TPC-H queries.
QUERY_RANGE=${6:-1..22}

if [[ $ENVIRO -ne "wan" ]]; then
    BATCH_SIZE=-12
    COMM_THREADS=4
else
    BATCH_SIZE=-1
    COMM_THREADS=-1
fi

COMM=nocopy

if [[ $ENVIRO == "same" ]]; then
    COMM=mpi
fi


PROFILING_MODE=1

if [[ $PROFILING_MODE -eq 1 ]]; then
    CMAKE="-DTRIPLES=ZERO -DEXTRA=-DQUERY_PROFILE=1"
else
    echo "WARNING: Profiling mode is OFF."
    CMAKE="-DEXTRA="
fi

if [[ $EXP_TYPE == "tpch" ]]; then
    query_list=$(eval echo q{$QUERY_RANGE})
    echo $query_list
elif [[ $EXP_TYPE == "other" ]]; then
    query_list=$(ls ../bench/queries/other/*.cpp | xargs -n1 basename | sed 's/\.cpp//')
elif [[ $EXP_TYPE == "secretflow" ]]; then

    # Force profiling mode 0 for secretflow queries
    PROFILING_MODE=0

    # secretflow experiments
    query_list=$(echo sf-{q1,q6,q12,q14,q1-mod})

    # confirm secretflow setup:
    #   - protocol 2
    #   - prorofiling mode OFF (we use SQL for plaintext)
    #   - LOCAL environment

    # make sure protocol is 2
    if [[ $PROTOCOL -ne 2 ]]; then
        echo "Must use 2PC for SecretFlow."
        exit 1
    fi

    # make sure not in profiling mode
    if [[ $PROFILING_MODE -ne 0 ]]; then
        echo "Turn off profiling mode before running SecretFlow queries."
        exit 1
    fi

    if [[ $ENVIRO -ne "same" ]]; then
        echo "SecretFlow experiments must be run in SAME environment."
        exit 1
    fi

    CMAKE="-DTRIPLES=ZERO -DEXTRA="
else
    usage
    exit 1
fi

if [[ $ENVIRO == "wan" ]]; then
    ./comm/cluster-wan-sim.sh on node{1,2,3}
    CMAKE="$CMAKE,-DWAN_CONFIGURATION"
else
    ./comm/cluster-wan-sim.sh off node{1,2,3}
    CMAKE=
fi

folder="../results/query-benchmark/$EXP_TYPE/$(date +%m%d-%H%M)-${PROTOCOL}PC-$ENVIRO-SF${SCALE_FACTOR}"
mkdir -p $folder
mkdir -p $folder/raw_data

cat <<EOF > $folder/meta.txt
Timestamp: $(date)

Git commit: $(git rev-parse HEAD)

Git status: $(git status)

Command line: $0 $*

Args:
EXP_TYPE=$EXP_TYPE
SCALE_FACTOR=$SCALE_FACTOR
THREADS=$THREADS
ENVIRO=$ENVIRO
BATCH_SIZE=$BATCH_SIZE
COMM=$COMM
COMM_THREADS=$COMM_THREADS
PROFILING_MODE=$PROFILING_MODE
QUERY_RANGE=$QUERY_RANGE

Network status:
$(ip a)

Simulator status:
$(tc qdisc)

CPU: ($(hostname))
$(lscpu)
EOF

first=1
for query in $query_list; do
    # if not first, sleep a bit to prevent socket issues
    if [[ $first -eq 0 ]]; then
        sleep 30
        first=0
    fi

    output_file=$folder/raw_data/$query.txt

    echo -e "Query $query | Scale Factor $SCALE_FACTOR | Protocol $PROTOCOL | Threads $THREADS\n" | tee -a $output_file

    ./run_experiment.sh -e 1 -m "${CMAKE}" -x node -p $PROTOCOL -s $ENVIRO \
        -c $COMM -n $COMM_THREADS -f $SCALE_FACTOR -T $THREADS -b $BATCH_SIZE \
        $EXTRA $query 2>&1 | tee -a $output_file
    
    if [[ $first -eq 1 ]]; then
        ( cd ../build; make -j tpch-queries )
    fi

    first=0
done

if [[ $ENVIRO == "wan" ]]; then
    ./comm/cluster-wan-sim.sh off node{1,2,3}
fi
