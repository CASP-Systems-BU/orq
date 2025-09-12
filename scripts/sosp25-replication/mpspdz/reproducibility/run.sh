#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <Number of Parties> <shuffle/sort> <first> <last>"
    exit 1
fi

# Read the arguments
num_parties=$1
script=$2
first=$3
last=$4

# Ensure arguments are valid
if ! [[ "$num_parties" =~ ^[0-9]+$ ]]; then
    echo "Number of Parties must be a positive integer."
    exit 1
fi

if ! [[ "$first" =~ ^-?[0-9]+$ && "$last" =~ ^-?[0-9]+$ ]]; then
    echo "Both 'first' and 'last' must be integers."
    exit 1
fi

# Map the number of parties to a protocol
case $num_parties in
    2) protocol="semi2k" ;;
    3) protocol="ring" ;;
    4) protocol="rep4-ring" ;;
    *) 
        echo "Unsupported Number of Parties: $num_parties"
        echo "Supported values are 2, 3, or 4."
        exit 1
        ;;
esac

Scripts/setup-ssl.sh

# Generate the list of integers and process each
for i in $(seq $first $last); do
    input_size=$((2 ** i))  # Calculate 2^i

    # if 2PC, add extra "-- -F" for dummy preprocessing
    flag=""
    [ "$num_parties" -eq 2 ] && flag="-- -F"

    for j in {1..3}; do
	sleep 15
    	echo "Iteration $j: Exponent $i"
    	Scripts/compile-run.py -H HOSTS -E $protocol -v -t $script $input_size $flag
    done

    echo "---"  # Separator for clarity
done
