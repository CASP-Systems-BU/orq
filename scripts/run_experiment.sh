#!/usr/bin/env bash

trap "exit 1" SIGINT

# Defaults
exp_protocol="3"
exp_setting="same"
exp_communicator="nocopy"
num_comm_threads="-1"  # Only used for NoCopyComm
batch_size=-12
row_exponents=20
scale_factor=-1
min_threads_pow=0
max_threads_pow=0
THREADS=1
exp_repetitions=1
cmake_args=""
exp_args=""
node_prefix="node"

use_scale_factor=0
use_power_of_10=false


usage () {
    echo "Usage: ${0} [options] <exp_name>"
    echo "  exp_name Experiment to run"
    echo "OPTIONS:"
    echo "  [-h]                                    Show this help"
    echo "  [-p 1|2|3|4]                            Protocol; default: ${exp_protocol}"
    echo "  [-s same|lan|wan]                       Setting; default: ${exp_setting}"
    echo "  [-c mpi|nocopy]                         Communicator; default: mpi for same; ${exp_communicator} otherwise"
    echo "  [-n num_comm_threads]                   [NoCopyComm only] Number of communicator threads (negative: # per worker); default: ${num_comm_threads}"
    echo "  [-r min_rows_pow[-max_rows_pow]]        Number of rows, as powers of 2, can be a range; default: ${row_exponents}"
    echo "  [-d]                                    Use powers of 10 for the number of rows flag (-r)"
    echo "  [-f scale_factor]                       Scale factor for TPC-H and other queries. Overrides -r if set."
    echo "  [-t min_threads_pow[-max_threads_pow]]  Number of threads, as powers of 2, can be a range"
    echo "  [-T threads]                            Number of threads (arbitrary); default: ${THREADS}"
    echo "  [-b batch_size]                         Batch size; default: ${batch_size}"
    echo "  [-e exp_repetitions]                    Number of times to repeat each rows/threads pairing; default: {exp_repetitions}"
    echo "  [-m cmake_args]                         Pass additional arguments to cmake (can be repeated for more)"
    echo "  [-a experiment_args]                    Pass additional arguments to the experiment binary (can be repeated for more)"
    echo "  [-x node prefix]                        Prefix for remote nodes. Machines are prefix0, prefix1, ...; default: ${node_prefix}"
    exit 1
}

(( $# < 1 )) && usage


cwd=$(pwd)

parse_range() {
    local arg="$1"
    local result=()

    # Split by commas first
    IFS=',' read -ra parts <<< "$arg"

    for part in "${parts[@]}"; do
        if [[ "$part" =~ ^([0-9]+)-([0-9]+)$ ]]; then
            # Handle range x-y
            result+=($(seq "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"))
        else
            # Handle individual value
            result+=("$part")
        fi
    done

    echo "${result[@]}"
}

specified_communicator=0

# Read options
RANGE_REGEX='^([0-9]+)(-([0-9]+))?$'
while getopts "p:s:c:n:r:df:t:e:m:a:x:b:hT:" arg; do
    case "${arg}" in
        p)
            exp_protocol=${OPTARG}
            (( exp_protocol >= 1 && exp_protocol <= 4 )) || { echo "Invalid protocol"; usage; }
            ;;
        s)
            exp_setting="${OPTARG}"
            [[ ${exp_setting} = "same" || ${exp_setting} = "lan" || ${exp_setting} = "wan" ]] || { echo "Invalid setting"; usage; }
            ;;
        c)
            exp_communicator="${OPTARG}"
            specified_communicator=1
            [[ ${exp_communicator} = "mpi" || ${exp_communicator} = "nocopy" ]] || { echo "Invalid communicator"; usage; }
            ;;
        n)
            num_comm_threads="${OPTARG}"
            ;;
        r)
            row_exponents=$(parse_range "$OPTARG")
            # [[ ${OPTARG} =~ ${RANGE_REGEX} ]] || { echo "Invalid rows"; usage; }
            # min_rows_pow=${BASH_REMATCH[1]}
            # max_rows_pow=${BASH_REMATCH[3]}
            # [[ -z ${max_rows_pow} ]] && max_rows_pow=${min_rows_pow}
            ;;
        d)
            use_power_of_10=true
            ;;
        f)
            scale_factor="${OPTARG}"

            if ! [[ $(echo "$scale_factor > 0" | bc) -eq 1 ]]; then
                echo "Error: Scale factor must be greater than zero."
                usage
                exit 1
            fi

            use_scale_factor=1
            ;;
        t)
            [[ ${OPTARG} =~ ${RANGE_REGEX} ]] || { echo "Invalid threads"; usage; }
            min_threads_pow=${BASH_REMATCH[1]}
            max_threads_pow=${BASH_REMATCH[3]}
            [[ -z ${max_threads_pow} ]] && max_threads_pow=${min_threads_pow}
            ;;
	    T)
 	        THREADS=$OPTARG
            ;;
        e)
            exp_repetitions=${OPTARG}
            (( exp_repetitions >= 1 )) || { echo "Invalid repetitions"; usage; }
            ;;
        m)
            cmake_args="${cmake_args} ${OPTARG}"
            ;;
        x)
            node_prefix="${OPTARG}"
            ;;
        a)
            exp_args="${exp_args} ${OPTARG}"
            ;;
        n)
            NODES="$OPTARG"
            ;;
        b)
            batch_size="$OPTARG"
            ;;
        h)
            usage
            ;;
    esac
done
shift $((OPTIND-1))

# default mpi for same
if [[ $specified_communicator == 0 && $exp_setting == "same" ]]; then
    exp_communicator="mpi"
fi

if [[ $use_scale_factor -eq 1 ]]; then
    exp_input_sizes=$scale_factor
else
    exp_input_sizes=$row_exponents
fi

# Read parameters
(( $# < 1 )) && { echo "Missing exp_name"; usage; }
exp_name=${1}

if [[ $exp_setting != "same" ]]; then
    iface=""
    # figure out which interface to use
    # for now, just check node1
    _node=${node_prefix}1
    _iface=$(ip route get $(dig +short ${_node}) | grep -Po "((?<=dev )\S*)")

    if [[ -n "$iface" && "$_iface" != "$iface" ]]; then
        echo "interface error: ${_node} routable via $_iface but previous interface(s) were routable via $iface".
        exit 1
    fi

    iface=$_iface

    SUBNET=$(ip -o -f inet addr show ${iface} | awk '{print $4}')

    [[ -n $iface ]] && echo "Common interface: ${iface}; subnet ${SUBNET}"
fi

# Set communicator commands
if [[ "$exp_communicator" == "mpi" ]]; then
    if [[ ${exp_setting} == "same" ]]; then
        RUN_CMD="mpirun"
    else
        RUN_CMD="mpirun --mca btl_tcp_if_include $SUBNET --mca oob_tcp_if_include $SUBNET"
    fi

    NUM_PROCESS_FLAG="-n"
    HOST_FLAG="--host"
    comm_cmake_arg="MPI"
elif [[ "$exp_communicator" == "nocopy" ]]; then
    RUN_CMD="startmpc"
    NUM_PROCESS_FLAG="-n"
    HOST_FLAG="-h"
    comm_cmake_arg="NOCOPY"
else
    echo "ERROR: Unsupported communicator: $exp_communicator" >&2
    exit 1
fi

# Build experiment
mkdir -p ../build
cd ../build
if [ $exp_communicator == "nocopy" ]; then
    cmake_args="${cmake_args} -DCOMM_THREADS=${num_comm_threads}"
fi

set -e
cmake .. -DPROTOCOL=${exp_protocol} -DCOMM=${comm_cmake_arg} ${cmake_args}
make -j ${exp_name}
set +e

if [[ $exp_setting != "same" ]]; then
    # Send binary to other machines
    for (( i=1; i<exp_protocol; i++ )); do
        scp ${cwd}/../build/${exp_name} ${node_prefix}${i}:${cwd}/../build
    done
fi

# Prepare output directory
mkdir -p ../results/${exp_setting}

# Generate command prefix
EXP_CMD_PREFIX=""
if (( exp_protocol > 1 )); then
    EXP_CMD_PREFIX="${RUN_CMD} ${NUM_PROCESS_FLAG} ${exp_protocol}"
    if [[ $exp_setting != 'same' ]]; then
        EXP_CMD_PREFIX="${EXP_CMD_PREFIX} ${HOST_FLAG} "
        for (( i=0; i<exp_protocol-1; i++ )); do
            EXP_CMD_PREFIX="${EXP_CMD_PREFIX}${node_prefix}${i},"
        done
        EXP_CMD_PREFIX="${EXP_CMD_PREFIX}${node_prefix}${i}"
    fi
else
    [[ ${exp_setting} != "same" ]] && echo "WARNING: settings other than 'same' have no effect for 1pc" >&2
fi

# Run experiment
for rexp in $exp_input_sizes; do
    
    if [[ $use_scale_factor -eq 1 ]]; then
        EXP_INPUT=$scale_factor
        input_suffix="SF"
    else
        if [[ $use_power_of_10 == true ]]; then
            EXP_INPUT=$((10 ** rexp))
        else
            EXP_INPUT=$((1 << rexp))
        fi
        input_suffix="rows"
    fi

    if [[ $THREADS == 0 ]]; then
        for (( THREAD_EXP=min_threads_pow; THREAD_EXP<=max_threads_pow; THREAD_EXP++ )) do
            THREADS=$((1 << THREAD_EXP))
            EXP_CMD="./${exp_name} ${THREADS} 1 ${batch_size} ${EXP_INPUT} ${exp_args}"
            echo "==== ${EXP_INPUT} ${input_suffix}; ${THREADS} threads ===="
            for (( REP=0; REP<exp_repetitions; REP++ )); do
                ${EXP_CMD_PREFIX} ${EXP_CMD}
            done
        done
    else
        EXP_CMD="./${exp_name} ${THREADS} 1 ${batch_size} ${EXP_INPUT} ${exp_args}"
        echo "==== ${EXP_INPUT} ${input_suffix}; ${THREADS} threads ===="
        for (( REP=0; REP<exp_repetitions; REP++ )); do
            ${EXP_CMD_PREFIX} ${EXP_CMD}
            if ((REP + 1 < exp_repetitions )); then
                sleep 31
            fi
        done
    fi

done
