#!/usr/bin/env bash
#
# Profile table sort.

THREADS=(1 2 4 8 12 16 24 32 48 64)

# A few powers of two, and then arithmetic mean points between
ROWS=(65536 163840 262144 655360 1048576 2621440 4194304 10485760 16777216)

for T in ${THREADS[@]}; do
    for R in ${ROWS[@]}; do
        ../scripts/run_experiment.py -p 3 -s lan -T $T -f $R micro_tablesort
    done
done