#!/bin/bash

# stop if any test fails.
set -e

cd $(dirname $0)

cmake ../.. -DTRIPLES=DUMMY

./run_multithreaded_test.sh 2 1
./run_multithreaded_test.sh 2 4

./run_multithreaded_test.sh 3 1
./run_multithreaded_test.sh 3 4

./run_multithreaded_test.sh 4 1
./run_multithreaded_test.sh 4 4