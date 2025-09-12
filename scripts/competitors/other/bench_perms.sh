# Check if the correct number of arguments is provided
if [ "$#" -ne 3 ]; then
    echo "Usage: $0 <bitwidth> <first> <last>"
    exit 1
fi

bitwidth=$1
first=$2
last=$3

if ! [[ "$first" =~ ^-?[0-9]+$ && "$last" =~ ^-?[0-9]+$ ]]; then
    echo "Both 'first' and 'last' must be integers."
    exit 1
fi

# Generate the list of integers and process each
for i in $(seq $first $last); do
    result=$((2 ** i))  # Calculate 2^i
    for j in {1..3}; do
	    echo "Iteration $j: Exponent $i"
        python3 preprocess_permcorrs.py $result 1 1 $bitwidth
    done
    echo "---"  # Separator for clarity
done