#!/usr/bin/env bash

trap "exit 1" SIGINT

REPS=3
INPUT_SIZES=13,14,16,17,20,23,24,26,27,29
CMAKE="-DDEFAULT_BITWIDTH=64"
ENVIRO=wan

cd $(dirname $0)

folder="../results/prim-bench/$(date +%m%d-%H%M)"
mkdir -p $folder

[[ $ENVIRO == "wan" ]] && ./cluster-wan-sim.sh on node{1,2,3}

cat <<EOF > $folder/meta.txt
Timestamp: $(date)

Git commit: $(git rev-parse HEAD)

Git status: $(git status)

Command line: $0 $*

Args:
REPS=$REPS
INPUT_SIZES=$INPUT_SIZES
CMAKE=$CMAKE
ENVIRO=$ENVIRO

Hostnames: $(hostname); $(hostname -A)

Network status:
$(ip a)

Simulator status:
$(tc qdisc)

CPU: ($(hostname))
$(lscpu)
EOF

for p in 2 3 4; do
    echo "== Protocol: $p ($ENVIRO) =="
    ./run_experiment.sh -p $p -e $REPS -s $ENVIRO -m $CMAKE -r $INPUT_SIZES -x node micro_primitives 2>&1 | tee $folder/p${p}-${ENVIRO}.txt
done

[[ $ENVIRO == "wan" ]] && ./cluster-wan-sim.sh off node{1,2,3}

