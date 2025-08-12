#!/usr/bin/env bash

cd $(dirname $0)

CONTROL=$1

shift

NODES=$*

me=$(hostname --fqdn)

for n in $NODES
do
    echo $me
    ./wan-sim.py $CONTROL -H $n

    # assume nodeX access node0 with same interface as it does nodeY
    scp ./wan-sim.py $n:~/
    ssh $n ./wan-sim.py $CONTROL -H $me
done

# sanity check
for n in $NODES
do
    ping -qc 3 $n
    echo
done