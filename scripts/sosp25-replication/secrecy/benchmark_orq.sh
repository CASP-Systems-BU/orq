#!/usr/bin/env bash

cd $(dirname $0)

NODE_PREFIX="node"
if [[ $# -gt 0 ]]; then
    NODE_PREFIX="$1"
fi

REPS=1

# Example: bash ../results/run-experiment.sh 
EXP_HOME_DIR=$(pwd)/../../../results/secrecy/orq
EXP_PREFIX="../scripts/run_experiment.sh -x $NODE_PREFIX -e $REPS -p 3 -s lan -c nocopy -n 4 -T 16"

cd ../../../build

mkdir -p $EXP_HOME_DIR

############################################################################
## The first three queries run at slightly different table-size ratios in
## Secrecy vs ORQ, so we have to use a special flag.

cmake .. -DEXTRA=-DSECRECY_QUERY

# Q13 (exp_tpch_q13): Orders 256k, customers 32k
# customersSize() -> 32768
# ordersSize() -> 262144
$EXP_PREFIX -f 0.17476267 -b 65536 q13 | tee $EXP_HOME_DIR/exp_tpch_q13.txt

# Comorbidity (exp_q1): first table 2m, second table 256
# cohortSize(scaleFactor) -> 256
# diagnosisSize(scaleFactor) -> 2097152
# TODO: change batch to -2
$EXP_PREFIX -f 1 -b 65536 comorbidity | tee $EXP_HOME_DIR/exp_q1.txt

# Aspirin count (exp_q3): input tables 32k 
# getDiagnosisTable::size_t S -> 32768;
# getMedicationTable::size_t S -> 32768;
# ORQ SF1 = 2M for the smaller table.
$EXP_PREFIX -f 0.016384 -b 65536 aspirin | tee $EXP_HOME_DIR/exp_q3.txt

# Unset the flag.
cmake .. -DEXTRA=

############################################################################
## The remaining queries run at properly adjustable sizes

# Save time by precompiling
make -j rcdiff q4 q6 credit_score pwd-reuse

# Rec. cdiff (exp_q2) : first table 2m
# getDiagnosisTable::size_t S -> 2097152;
# ORQ SF1 = 3M, so run at SF 0.699
$EXP_PREFIX -f 0.69905067 -b 65536 rcdiff | tee $EXP_HOME_DIR/exp_q2.txt

# Q4 (exp_tpch_q4): Lineintems 128k, orders 32k
# ordersSize() -> 32768
# ORQ SF1 = 7.5M combined
$EXP_PREFIX -f 0.02184533 -b 65536 q4 | tee $EXP_HOME_DIR/exp_tpch_q4.txt

# Q6 (exp_tpch_q6): 8m
# ordersSize() -> 2097152
# ORQ SF1 = 6M
$EXP_PREFIX -f 1.39810133 -b 65536 q6 | tee $EXP_HOME_DIR/exp_tpch_q6.txt

# Credit Score (exp_qcredit): input table 2m
# getCreditScoreTable::size_t S -> 2097152;
# ORQ SF1 = 5M
$EXP_PREFIX -f 0.4194304 -b 65536 credit_score | tee $EXP_HOME_DIR/exp_qcredit.txt

# Password Reuse (exp_qpwd): input table 2m
# getPasswordTable::size_t S -> 2097152;
# ORQ SF1 = 5M
$EXP_PREFIX -f 0.4194304 -b 65536 pwd-reuse | tee $EXP_HOME_DIR/exp_qpwd.txt
