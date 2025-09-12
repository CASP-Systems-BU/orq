#!/usr/bin/env bash
#
# Run from build.


cmake .. -DPROTOCOL=1 -DEXTRA="-DQUERY_PROFILE"
make -j tpch-queries

SF=1

mkdir -p bandwidth-$SF

for q in q{1..22}; do
    (
        echo "==========================="
        echo "==== START Query $q SF$SF"
        echo "==========================="
        ./$q 1 1 -1 $SF | tee bandwidth-$SF/$q.txt
    ) &
done

wait

echo "Done."
