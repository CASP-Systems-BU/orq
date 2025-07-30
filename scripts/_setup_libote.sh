#!/bin/bash

# abort if any command fails
set -e

## Set up libOTe
echo "==== Building libOTe..."

mkdir -p libOTe-install

if ! cd libOTe; then
    # Feb 2025: switching to master branch
    echo "NOTE: cloning from fork."
    git clone https://github.com/elimbaum/libOTe.git
    cd libOTe
    # Latest commit from master branch
    # This file needs to change with every new commit to trigger a new build on GitHub CI
    git checkout 587b325d2dfcab27a70f7954adc99a5cd65712d8
fi

if [[ $(uname -m) == x86_64 ]]; then
    SSE_OPTION="-DENABLE_SSE=ON"
else
    SSE_OPTION="-DENABLE_SSE=OFF"
fi

python3 build.py --par=4 --boost --sodium --all --install=../libOTe-install \
    $SSE_OPTION

echo "==== libOTe installed"
