#!/usr/bin/env bash

# Usage function
usage() {
    echo "Usage: $0 [NODE_PREFIX]"
    echo ""
    echo "Setup and run Secrecy benchmarks across multiple nodes."
    echo "This script will:"
    echo "  1. Setup Secrecy on the local node"
    echo "  2. Setup Secrecy on remote nodes via SSH"
    echo "  3. Run the benchmarks"
    echo "  4. Extract the results"
    echo ""
    echo "Arguments:"
    echo "  NODE_PREFIX    Optional prefix for node hostnames (default: 'node')"
    echo "                 Nodes will be named as: {PREFIX}0, {PREFIX}1, {PREFIX}2"
    echo ""
    echo "Examples:"
    echo "  $0              # Uses default prefix 'node' -> node0, node1, node2"
    echo "  $0 machine-     # Uses prefix 'machine-' -> machine-0, machine-1, machine-2"
    echo ""
    echo "Note: Make sure SSH access is configured for the remote nodes."
}

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

cd $(dirname $0)

# local node setup
( ./setup_secrecy.sh )

# Setup secrecy on all other nodes
ssh ${NODE_PREFIX}1 'bash -s' < ./setup_secrecy.sh
ssh ${NODE_PREFIX}2 'bash -s' < ./setup_secrecy.sh

# Run the benchmarks
./benchmark_secrecy.sh "$NODE_PREFIX"

# Extract results
./extract_results_secrecy.sh