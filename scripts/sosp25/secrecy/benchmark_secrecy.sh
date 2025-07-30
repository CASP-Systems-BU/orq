#!/usr/bin/env bash

cd $(dirname $0)

# Usage function
usage() {
    echo "Usage: $0 [NODE_PREFIX]"
    echo ""
    echo "Run Secrecy benchmarks across multiple nodes using MPI."
    echo ""
    echo "Arguments:"
    echo "  NODE_PREFIX    Optional prefix for node hostnames (default: 'node')"
    echo "                 Nodes will be named as: {PREFIX}0, {PREFIX}1, {PREFIX}2"
    echo "Results will be saved to: results/secrecy/"
}

# Parse command line arguments
if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    usage
    exit 0
fi

# Defaults
NODE_PREFIX="node"
if [[ $# -gt 0 ]]; then
    NODE_PREFIX="$1"
fi

EXP_HOSTS="-n 3 -host ${NODE_PREFIX}0,${NODE_PREFIX}1,${NODE_PREFIX}2"
SECRECY_BUILD_DIR=~/Secrecy/build
EXP_HOME_DIR=$(pwd)/../../../results/secrecy/secrecy

# Create results directory
mkdir -p $EXP_HOME_DIR

# Change to the build directory
cd $SECRECY_BUILD_DIR

# Setting up the SUBNET variable
iface=""
# figure out which interface to use
# for now, just check node1
_node=${NODE_PREFIX}1
_iface=$(ip route get $(dig +short ${_node}) | grep -Po "((?<=dev )\S*)")
if [[ -n "$iface" && "$_iface" != "$iface" ]]; then
    echo "interface error: ${_node} routable via $_iface but previous interface(s) were routable via $iface".
    exit 1
fi
iface=$_iface
SUBNET=$(ip -o -f inet addr show ${iface} | awk '{print $4}')
[[ -n $iface ]] && echo "Common interface: ${iface}; subnet ${SUBNET}"

RUN_CMD="mpirun --mca btl_tcp_if_include $SUBNET --mca oob_tcp_if_include $SUBNET"
EXP_PREFIX="$RUN_CMD $EXP_HOSTS"

# Q4 (exp_tpch_q4): Lineintems 128k, oders 32k, batch size 8k
date
$EXP_PREFIX ./exp_tpch_q4 32768 131072 8192 | tee $EXP_HOME_DIR/exp_tpch_q4.txt

# Q6 (exp_tpch_q6): 8m
date
$EXP_PREFIX ./exp_tpch_q6 8388608 | tee $EXP_HOME_DIR/exp_tpch_q6.txt

# Q13 (exp_tpch_q13): Orders 256k, customers 32k, 4k batch size
date
$EXP_PREFIX ./exp_tpch_q13 32768 262144 4096 | tee $EXP_HOME_DIR/exp_tpch_q13.txt

# Comorbidity (exp_q1): first table 2m, second table 256, TODO: check on second input
date
$EXP_PREFIX ./exp_q1 2097152 256 | tee $EXP_HOME_DIR/exp_q1.txt

# Rec. cdiff (exp_q2) : first table 2m
date
$EXP_PREFIX ./exp_q2 2097152 | tee $EXP_HOME_DIR/exp_q2.txt

# Aspirin count (exp_q3): input tables 32k , batch size 32k
date
$EXP_PREFIX ./exp_q3 32768 32768 16384 | tee $EXP_HOME_DIR/exp_q3.txt

# Credit Score (exp_qcredit): input table 2m, batch size 256k
date
$EXP_PREFIX ./exp_qcredit 2097152 262144 | tee $EXP_HOME_DIR/exp_qcredit.txt

# Password Reuse (exp_qpwd): input table 2m, batch size 256k
date
$EXP_PREFIX ./exp_qpwd 2097152 262144 | tee $EXP_HOME_DIR/exp_qpwd.txt