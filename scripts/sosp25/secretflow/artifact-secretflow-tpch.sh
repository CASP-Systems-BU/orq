set -e

cd $(dirname $0)

RESULT_PATH="../../../results/secretflow-tpch/"
mkdir -p "$RESULT_PATH"

RESULT_DIR=$(realpath "$RESULT_PATH")

if [[ $# -eq 1 && "$1" == "plot" ]]; then
    python3 plot-secretflow-queries.py $RESULT_DIR/
    mv secretflow-queries.png $RESULT_DIR/

    echo -e "\nPlot directory: $RESULT_DIR"
    exit 0
elif [[ $# -eq 1 && "$1" == "setup" ]]; then
    cd helper

    # Setup Secretflow TPC-H query experiment
    ./secretflow-tpch-setup.sh

    echo -e "\nSecretFlow TPC-H experiment setup completed."
    exit 0
fi

cd helper

# Run Secretflow TPC-H query experiment
./secretflow-tpch-runner.sh "$RESULT_DIR"

# Run ORQ TPC-H query experiment
./orq-tpch-runner.sh "$RESULT_DIR"
