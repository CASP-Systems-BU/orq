#!/usr/bin/env bash

# Change to the home directory
cd ~

# If a directory called Secrecy does not exist, clone it
if [ ! -d "Secrecy" ]; then
    #  Clone the Secrecy repository
    git clone https://github.com/CASP-Systems-BU/Secrecy.git

    # Change to the Secrecy directory
    cd ~/Secrecy

    # clone sql-parser if not already installed
    mkdir -p include/external-lib/
    cd ~/Secrecy/include/external-lib/
    git clone https://github.com/mfaisal97/sql-parser.git

    # create a build directory
    cd ~/Secrecy
    mkdir -p build
    cd build

    # build the project
    cmake ..
    # use nproc for number of parallel jobs/2
    make -j $(( $(nproc) / 2 ))
fi
