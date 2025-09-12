RESULT_DIR="$1"

# Simulating LAN since secretflow sorting is running locally
docker exec -it spu-dev-$(whoami) bash -c "tc qdisc del dev lo root"
docker exec -it spu-dev-$(whoami) bash -c "tc qdisc add dev lo root netem rate 10Gbit"

{
    echo -e "Sort: 100000\n"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=100000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=100000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=100000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=100000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=100000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=100000"

    echo -e "Sort: 1000000\n"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=1000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=1000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=1000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=1000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=1000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=1000000"

    echo -e "Sort: 10000000\n"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=10000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=10000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=10000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=10000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=10000000"
    docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=10000000"
} | tee sf-spu-lan.txt

docker exec -it spu-dev-$(whoami) bash -c "tc qdisc del dev lo root"

mv sf-spu-lan.txt $RESULT_DIR/
