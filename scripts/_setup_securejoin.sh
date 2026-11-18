#!/bin/bash

# abort if any command fails
set -e

## Set up SecureJoin
echo "==== Building SecureJoin..."

# Resolve before the clone step, which cds into secure-join.
script_dir=$(cd "$(dirname "$0")" && pwd)

mkdir -p secure-join-install

if ! cd secure-join; then
    git clone https://github.com/CASP-Systems-BU/secure-join.git
    cd secure-join
    # Latest commit from master branch
    # This file needs to change with every new commit to trigger a new build on GitHub CI
    git checkout 98fda2e6780738926bc53c90e801c449e3ba89e8
fi

case "$(uname -m)" in
    x86_64|amd64|AMD64)
        SSE_OPTION="-DENABLE_SSE=ON"
        ;;
    *)
        SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DSECUREJOIN_ENABLE_SSE=OFF"
        ;;
esac

# Use the libOTe already installed by _setup_libote.sh. --noauto tells
# SecureJoin not to clone and build another copy. The include file supplies
# the libOTe target name its CMakeLists already links.
libote_prefix="$(cd .. && pwd)/libOTe-install"
if [[ ! -f "${libote_prefix}/lib/cmake/libOTe/libOTeConfig.cmake" && ! -f "${libote_prefix}/lib64/cmake/libOTe/libOTeConfig.cmake" ]]; then
    echo "libOTe install not found under ${libote_prefix}; run _setup_libote.sh first" >&2
    exit 1
fi

# SecureJoin adds -march=native unless this is set. On GitHub's x86 runners
# that faults. Leave other machines on the project default.
ARCH_OPTION=
case "$(uname -m)" in
    x86_64|amd64|AMD64)
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            ARCH_OPTION="-DNO_ARCH_NATIVE=ON"
        fi
        ;;
esac

python3 build.py --noauto -DSODIUM_MONTGOMERY=false \
    -DLIBOTE_INSTALL_PREFIX="${libote_prefix}" \
    -DCMAKE_PROJECT_secureJoin_INCLUDE="${script_dir}/securejoin-installed-libote.cmake" \
    --install=../secure-join-install \
    $SSE_OPTION $ARCH_OPTION

echo "==== SecureJoin installed"
