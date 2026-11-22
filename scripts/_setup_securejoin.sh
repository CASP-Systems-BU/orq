#!/bin/bash

# abort if any command fails
set -e

## Set up SecureJoin against the libOTe install from scripts/_setup_libote.sh.
echo "==== Building SecureJoin..."

SECUREJOIN_REPO="https://github.com/CASP-Systems-BU/secure-join.git"
# This file needs to change with every new commit to trigger a new build on GitHub CI
SECUREJOIN_COMMIT="86ca35dc81e086e5bbee15cb4eebab770afb5971"

script_dir=$(cd "$(dirname "$0")" && pwd)
# Invoked from the build directory, same as the other setup scripts.
libote_prefix="$(pwd)/libOTe-install"
if [[ ! -f "${libote_prefix}/lib/cmake/libOTe/libOTeConfig.cmake" \
   && ! -f "${libote_prefix}/lib64/cmake/libOTe/libOTeConfig.cmake" ]]; then
    echo "libOTe is not installed at ${libote_prefix}. Run scripts/_setup_libote.sh first." >&2
    exit 1
fi

mkdir -p secure-join-install

if [ ! -d secure-join ]; then
    git clone "$SECUREJOIN_REPO" secure-join
fi
cd secure-join

git fetch origin
git checkout "$SECUREJOIN_COMMIT"

if [[ $(uname -m) == x86_64 || $(uname -m) == amd64 || $(uname -m) == AMD64 ]]; then
    SSE_OPTION="-DENABLE_SSE=ON -DSECUREJOIN_ENABLE_SSE=ON"
else
    SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DSECUREJOIN_ENABLE_SSE=OFF"
fi

# A cached CI binary must not be compiled -march=native. That flag is what
# produced illegal-instruction failures when the cache moved between runners.
NATIVE_OPTION=
if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
    NATIVE_OPTION="-DNO_ARCH_NATIVE=ON"
fi

# libOTe's headers need <format>, and SecureJoin is C++20.
if ! echo '#include <format>' | ${CXX:-g++} -std=c++20 -x c++ -E - >/dev/null 2>&1; then
    if command -v g++-13 >/dev/null 2>&1; then
        export CXX=g++-13
        export CC=gcc-13
    else
        echo "SecureJoin requires GCC 13 or newer (missing <format>). Install g++-13 and rerun." >&2
        exit 1
    fi
fi

# --noauto is SecureJoin's switch for "use a libOTe that is already installed."
# build.py does not check cmake's exit status, so the install is checked below.
python3 build.py --noauto \
    -DCMAKE_PREFIX_PATH="${libote_prefix}" \
    -DCMAKE_PROJECT_secureJoin_INCLUDE="${script_dir}/cmake/securejoin-libote.cmake" \
    -DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON \
    -DNO_CMAKE_SYSTEM_PATH=ON \
    -DSODIUM_MONTGOMERY=false \
    -DCMAKE_CXX_FLAGS=-DBOOST_ASIO_DISABLE_SERIAL_PORT \
    --install=../secure-join-install \
    $SSE_OPTION $NATIVE_OPTION

install_prefix="$(pwd)/../secure-join-install"
if [[ ! -f "${install_prefix}/lib/libsecureJoin.a" \
   && ! -f "${install_prefix}/lib64/libsecureJoin.a" ]]; then
    echo "SecureJoin did not install libsecureJoin.a" >&2
    exit 1
fi
if [[ ! -f "${install_prefix}/include/secureJoin/secure-join/Prf/AltModPrf.h" ]]; then
    echo "SecureJoin did not install its headers" >&2
    exit 1
fi

echo "==== SecureJoin installed"
