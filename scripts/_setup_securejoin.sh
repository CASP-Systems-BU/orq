#!/bin/bash

# abort if any command fails
set -e

## Set up libOTe
echo "==== Building SecureJoin..."

mkdir -p secure-join-install

if ! cd secure-join; then
    git clone https://github.com/CASP-Systems-BU/secure-join.git
    cd secure-join
    # Latest commit from master branch
    # This file needs to change with every new commit to trigger a new build on GitHub CI
    git checkout a931c428442456bfee415558e13c9c33484acef9
fi

if [[ $(uname -m) == x86_64 ]]; then
    SSE_OPTION="-DENABLE_SSE=ON"
else
    SSE_OPTION="-DENABLE_SSE=OFF"
fi

python3 build.py -D SODIUM_MONTGOMERY=false --install=../secure-join-install \
    $SSE_OPTION

echo "==== SecureJoin installed"
