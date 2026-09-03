#!/usr/bin/env bash

mkdir json-out

THR=16
REP=3
SETTING=lan

# run_4pc_exp <experiment> [size_arg]
# size_arg is passed through to run_experiment.py (e.g. "-f 1" or "-r 2^10").
# If omitted: queries (q*) default to "-f 1"; others use run_experiment defaults.
run_4pc_exp() {
    exp=$1
    size_arg=${2:-}
    explicit_size=0
    if [[ -n "${2:-}" ]]; then
        explicit_size=1
    fi

    if [[ -z "$size_arg" && $exp == q* ]]; then
        size_arg="-f 1"
    fi

    path=json-out/$exp
    # Nest by size only when the caller passed an explicit size (e.g. sorting sweep)
    if [[ $explicit_size -eq 1 ]]; then
        size_tag=$(echo "$size_arg" | tr -d ' -')
        path=json-out/$exp/$size_tag
    fi
    mkdir -p $path

    # Dalskov without check - baseline
    ../scripts/run_experiment.py -s $SETTING -o 3 -p 4 -T $THR -n 4 -e $REP \
        -m=-DEXTRA=-DSKIP_MALICIOUS_CHECK=1 \
        -m=-DUSE_DALSKOV_FANTASTIC_FOUR=ON \
        $size_arg $exp

    mv output.json $path/baseline.json

    # Standard 4PC - Dalskov with check
    ../scripts/run_experiment.py -s $SETTING -o 3 -p 4 -T $THR -n 4 -e $REP \
        -m=-DEXTRA= \
        $size_arg $exp

    mv output.json $path/fixed.json

    ../scripts/run_experiment.py -s $SETTING -o 3 -p 4 -T $THR -n 4 -e $REP \
        -m=-DUSE_DALSKOV_FANTASTIC_FOUR=OFF \
        $size_arg $exp

    mv output.json $path/custom.json
}

run_4pc_exp micro_sorting "-r 2^10,2^12,2^14,2^16,2^18,2^20,2^22,2^24,2^26,2^28"

run_4pc_exp q11
run_4pc_exp q22
run_4pc_exp q12
run_4pc_exp q8
run_4pc_exp q21
