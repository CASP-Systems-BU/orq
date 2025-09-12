#!/usr/bin/env bash
#
# $ git clone git@github.com:CASP-Systems-BU/orq.git
# $ cd orq
# $ git checkout sosp-artifact-eval

set -e

SCALE_FACTOR=1

cd $(dirname $0)

usage () {
    echo "Usage: $0 [plot | [<PROTOCOL: 2|3|4> <lan|wan> [query-spec]]"
    echo "  SH-DM  = 2PC"
    echo "  SH-HM  = 3PC"
    echo "  Mal-HM = 4PC"
    echo "  We expect node0, node1, ... to be routable"
    echo "  In WAN, we simulate a 20 ms, 6 Gbps connection"
    echo "  Queries will run at scale factor $SCALE_FACTOR."
    exit 1
}

if [[ $# -gt 3 ]]; then
    usage;
fi

PROTOCOL="$1"
NETWORK="$2"

case "$PROTOCOL" in
    2|3|4)
        # valid
        ;;
    plot)
        # run plot script!
        ROOT="../../results/query-benchmark/*"
        python3 ../plot_query_experiments.py \
            -L2 "$ROOT/*2PC-lan*" \
            -W2 "$ROOT/*2PC-wan*" \
            -L3 "$ROOT/*3PC-lan*" \
            -W3 "$ROOT/*3PC-wan*" \
            -L4 "$ROOT/*4PC-lan*" \
            -W4 "$ROOT/*4PC-wan*" \
            queries
        exit 0
        ;;
    *)
        echo "Error: Either \`plot\` or PROTOCOL must be 2, 3, or 4."
        usage;
esac

if [[ "$NETWORK" != "lan" && "$NETWORK" != "wan" ]]; then
    echo 'Error: NETWORK must be `lan` or `wan`.'
    usage;
fi

# Disable WAN sim for connectivity check & setup.
../comm/cluster-wan-sim.sh off node{1,2,3}

ping -qc 1 node1 && \
ping -qc 1 node2 && \
ping -qc 1 node3 && \
echo "==== Connectivity check OK! ====" || exit 1

if [[ ! -f ~/already-deployed ]]; then
    echo "==== Haven't deployed yet. ===="
    # shell expand to the full list of nodes
    ../orchestration/deploy.sh ~/orq/ node{0,1,2,3}
    echo "==== Installing python packages ===="
    pip install numpy pandas matplotlib
    
    # don't repeat installation
    if [[ $? -eq 0 ]]; then
        touch ~/already-deployed
    else
        echo "==== Failed to deploy! ===="
        exit 1
    fi
    echo "==== Deployment done. ===="
fi

(
    echo "==== Test nocopy... ==="
    cd ../../build
    ../scripts/run_experiment.sh -s $NETWORK -x node -p $PROTOCOL -c nocopy -T 1 -r 20 test_primitives
    echo "==== Test OK? Cancel if not. ===="
    sleep 1
)

# If not specified, this arg will be empty
QUERY_SELECT=$3

# Run queries with 16 threads.
echo "==== Start TPCH ===="
../query-experiments.sh tpch $SCALE_FACTOR $PROTOCOL 16 $NETWORK $QUERY_SELECT
echo "==== Finished TPCH ===="

echo "==== Start Other Queries ===="
../query-experiments.sh other $SCALE_FACTOR $PROTOCOL 16 $NETWORK
echo "==== Finished Other Queries ===="