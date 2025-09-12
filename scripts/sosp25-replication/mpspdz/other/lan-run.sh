#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <PartyID> <first> <last>"
    exit 1
fi

# Read the arguments
partyID=$1
first=$2
last=$3

# Ensure arguments are valid
if ! [[ "$partyID" =~ ^[0-9]+$ ]]; then
    echo "PartyID must be 0 or 1."
    exit 1
fi

if ! [[ "$first" =~ ^-?[0-9]+$ && "$last" =~ ^-?[0-9]+$ ]]; then
    echo "Both 'first' and 'last' must be integers."
    exit 1
fi

# Generate the list of integers and process each
for i in $(seq $first $last); do
    input_size=$((2 ** i))  # Calculate 2^i
    ./compile.py -R 64 Programs/Source/sort.mpc $input_size
    for j in {1..3}; do
	echo "Iteration $j: Exponent $i"
    	./semi2k-party.x -F -N 2 -h node0 $partyID "sort-${input_size}" -v
    done
    echo "---"  # Separator for clarity
done

