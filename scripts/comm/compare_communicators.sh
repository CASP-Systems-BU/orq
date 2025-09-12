#!/bin/bash

# Script to compare communicator performance for a given binary

verbose=0
while getopts "v" opt; do
  [ "$opt" = "v" ] && verbose=1
done
shift $((OPTIND -1))

if [ $# -lt 1 ]; then
    echo "Usage: $0 [-v] <binary> [<num_parties>]"
    exit 1
fi

function run_communicator {
    local communicator=$1
    local command=$2
    local protocol_num=$3
    local binary=$4

    echo "$communicator Communicator:"
    echo -e "\n$communicator Communicator:" >> result.txt

    echo " - Setting cmake args for communicator"
    cmake .. -DCOMM=$communicator -DPROTOCOL=$protocol_num > /dev/null 2>&1
    echo " - Making \"$binary\""
    make $binary > /dev/null 2>&1
    echo " - Running \"$binary\""
    { time $command ./$binary >> result.txt; } 2>&1 | sed 's/^/     /'
    echo
}

binary=$1
num_parties=${2:-3}

cd ../../build

echo -e "Running \"$binary\" with communicators (${num_parties}pc) ...\n"

run_communicator "MPI" "mpirun -np $num_parties" $num_parties $binary 
run_communicator "SOCKET" "startmpc -n $num_parties" $num_parties $binary

if [ $verbose -eq 1 ]; then
    cat result.txt
fi

rm result.txt
