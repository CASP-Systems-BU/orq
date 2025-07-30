#!/bin/bash

cd ..
# rm -rf build
mkdir build
cd build
cmake ..
make -j1

# make clean

MPIRUN="mpirun -np"
PARTIES=3
dirlist=`find ../tests/ -type f -exec basename {} \; | sed 's/\.[^.]*$//'`

for test in ${dirlist}
do
	echo "Running test:" $test
    make $test > /dev/null 2>&1;
    $MPIRUN $PARTIES ./$test
    echo "--------------------------------------"
done

cd ..