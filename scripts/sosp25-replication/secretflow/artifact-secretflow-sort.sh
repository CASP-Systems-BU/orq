set -e

cd $(dirname $0)

RESULT_PATH="../../../results/secretflow-sort/"
mkdir -p "$RESULT_PATH"

RESULT_DIR=$(realpath "$RESULT_PATH")

if [[ $# -eq 1 && "$1" == "plot" ]]; then
    python3 ../../old-paper/plot-secretflow-comparison.py
    mv ../../old-paper/secretflow-sorting.png $RESULT_DIR/

    echo -e "\nPlot directory: $RESULT_DIR"
    exit 0
fi

cd helper

# Setup Secretflow sorting experiment
./secretflow-sort-setup.sh

# Run Secretflow sorting experiment
./secretflow-sort-runner.sh "$RESULT_DIR"

# Run ORQ sorting experiment
./orq-sort-runner.sh "$RESULT_DIR"
