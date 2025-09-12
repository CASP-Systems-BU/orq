#!/usr/bin/env bash

cd $(dirname $0)

# Parse command line arguments
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

# defaults
NODE_PREFIX="node"
if [[ $# -gt 0 ]]; then
    NODE_PREFIX="$1"
fi

# Run the ORQ benchmarks
./benchmark_orq.sh "$NODE_PREFIX"

# Extract results
./extract_results_orq.sh