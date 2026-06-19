#!/bin/bash

# abort if any command fails
set -e

## Set up libOTe
echo "==== Building libOTe..."

source $(dirname $0)/_clone_libote.sh

if [[ $(uname -m) == x86_64 ]]; then
    SSE_OPTION="-DENABLE_SSE=ON"
else
    SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF "
fi

python3 build.py --par=4 --boost --sodium --relic -DSODIUM_MONTGOMERY=false --all --install=../libOTe-install \
    $SSE_OPTION

echo "==== libOTe installed"
