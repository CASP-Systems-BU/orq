#!/usr/bin/env bash
#
# Generate all plots for SOSP '25 paper.

OUTPUT=~/Desktop/tpch-data/output-plot

mv $OUTPUT/*.png $OUTPUT/old/
set -e

OLD_CWD=$(pwd)

# go to the top level directory
cd $(dirname $0)
cd "$(git rev-parse --show-toplevel || echo .)"
cd scripts/osdi25

python3 secrecy-comparison.py
mv secrecy-comp.png $OUTPUT/

python3 secretflow-queries-comparison.py
mv secflow-comp.png $OUTPUT/

python3 plot-secretflow-comparison.py
mv secretflow-comparison.png $OUTPUT/

(
    cd mpspdz
    python3 plot_mpspdz_sort.py
    mv mpspdz-compare.png $OUTPUT/
)

(
    cd sorting-main
    python3 plot_sorting.py
    mv plot-sort.png $OUTPUT/
)

cd $OLD_CWD

set +e