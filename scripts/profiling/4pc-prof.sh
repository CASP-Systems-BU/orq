#!/usr/bin/env bash

mkdir json-out

THR=16
REP=3

run_4pc_exp() {
    exp=$1
    path=json-out/$exp
    mkdir $path

    # for queries, run SF 1. Otherwise, use defaults
    if [[ $exp == q* ]]; then
        size_arg="-f 1"
    else
        size_arg=""
    fi


    # Dalskov without check - baseline
    ../scripts/run_experiment.py -s lan -o 3 -p 4 -T $THR -n 4 -e $REP \
        -m=-DEXTRA=-DSKIP_MALICIOUS_CHECK=1 \
        -m=-DUSE_DALSKOV_FANTASTIC_FOUR=ON \
        $size_arg $exp

    mv output.json $path/baseline.json

    # Standard 4PC - Dalskov with check
    ../scripts/run_experiment.py -s lan -o 3 -p 4 -T $THR -n 4 -e $REP \
        -m=-DEXTRA= \
        $size_arg $exp

    mv output.json $path/fixed.json

    ../scripts/run_experiment.py -s lan -o 3 -p 4 -T $THR -n 4 -e $REP \
        -m=-DUSE_DALSKOV_FANTASTIC_FOUR=OFF \
        $size_arg $exp

    mv output.json $path/custom.json
}

# TODO: figure out sizes.
run_4pc_exp micro_sorting
run_4pc_exp q11
run_4pc_exp q22
run_4pc_exp q12
run_4pc_exp q8
run_4pc_exp q21