#!/usr/bin/env bash

# Example: bash ../benchmarks/cost-model/secrecy-comparison/run-experiment.sh 
EXP_HOME=../benchmarks/cost-model/secrecy-comparison
EXP_PREFIX="../scripts/run_experiment.sh -x node -p 1 -s same -c nocopy -r 1 -T 1"

cd $(dirname $0)
cd ../../../build

mkdir -p $EXP_HOME

for ITNUM in 1
do
    # # Q4 (exp_tpch_q4): Lineintems 128k, oders 32k, batch size 64k
    # # ordersSize() -> 32768
    # date
    # $EXP_PREFIX -b 65536 q4 >> $EXP_HOME/exp_tpch_q4.txt


    # # Q6 (exp_tpch_q6): 8m
    # # ordersSize() -> 2097152
    # date
    # $EXP_PREFIX -b 65536 q6 >> $EXP_HOME/exp_tpch_q6.txt


    # # Q13 (exp_tpch_q13): Orders 256k, customers 32k, 4k batch size
    # # customersSize() -> 32768
    # # ordersSize() -> 262144
    # date
    # $EXP_PREFIX -b 65536 q13 >> $EXP_HOME/exp_tpch_q13.txt


    # # Comorbidity (exp_q1): first table 2m, second table 256, TODO: check on second input
    # # cohortSize(scaleFactor) -> 256
    # # diagnosisSize(scaleFactor) -> 2097152
    # # TODO: change batch to -2
    # date
    # $EXP_PREFIX -b 65536 comorbidity >> $EXP_HOME/exp_q1.txt


    # # Rec. cdiff (exp_q2) : first table 2m
    # # getDiagnosisTable::size_t S -> 2097152;
    # date
    # $EXP_PREFIX -b 65536 rcdiff >> $EXP_HOME/exp_q2.txt
    

    # # Aspirin count (exp_q3): input tables 32k , batch size 32k
    # # getDiagnosisTable::size_t S -> 32768;
    # # getMedicationTable::size_t S -> 32768;
    # date
    # $EXP_PREFIX -b 65536 aspirin >> $EXP_HOME/exp_q3.txt


    # # Credit Score (exp_qcredit): input table 2m, batch size 256k
    # # getCreditScoreTable::size_t S -> 2097152;
    # date
    # $EXP_PREFIX -b 65536 credit_score >> $EXP_HOME/exp_qcredit.txt

    # Password Reuse (exp_qpwd): input table 2m, batch size 256k
    # getPasswordTable::size_t S -> 2097152;
    date
    $EXP_PREFIX -b 65536 pwd-reuse >> $EXP_HOME/exp_qpwd.txt
done