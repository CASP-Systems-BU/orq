#!/bin/bash

# abort if any command fails
set -e

## Set up SecureJoin
echo "==== Building SecureJoin..."

SECUREJOIN_REPO="https://github.com/CASP-Systems-BU/secure-join.git"
# Latest commit from the update branch (official libOTe pin + API fixes).
# This file needs to change with every new commit to trigger a new build on GitHub CI
SECUREJOIN_COMMIT="86ca35dc81e086e5bbee15cb4eebab770afb5971"

mkdir -p secure-join-install

if [ ! -d secure-join ]; then
    git clone "$SECUREJOIN_REPO" secure-join
fi
cd secure-join

git fetch origin
git checkout "$SECUREJOIN_COMMIT"

if [[ $(uname -m) == x86_64 ]]; then
    SSE_OPTION="-DENABLE_SSE=ON"
else
    SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DSECUREJOIN_ENABLE_SSE=OFF"
fi

# Secure-join builds the pinned libOTe in-tree, so it needs the same compiler and flags.
if ! echo '#include <format>' | ${CXX:-g++} -std=c++20 -x c++ -E - >/dev/null 2>&1; then
    if command -v g++-13 >/dev/null 2>&1; then
        export CXX=g++-13
        export CC=gcc-13
    else
        echo "libOTe requires GCC 13 or newer (missing <format>). Install g++-13 and rerun." >&2
        exit 1
    fi
fi

python3 build.py -DSODIUM_MONTGOMERY=false -DENABLE_LOGVOLE=OFF -DENABLE_ALL_OT=ON --install=../secure-join-install \
    -DCMAKE_CXX_FLAGS=-DBOOST_ASIO_DISABLE_SERIAL_PORT \
    $SSE_OPTION

echo "==== SecureJoin installed"
