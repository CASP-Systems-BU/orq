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
    git checkout 98fda2e6780738926bc53c90e801c449e3ba89e8
fi

case "$(uname -m)" in
    x86_64|amd64|AMD64)
        SSE_OPTION="-DENABLE_SSE=ON"
        # secure-join adds -march=native unless this is set. On GitHub runners
        # that faults; leave native builds alone everywhere else.
        ARCH_OPTION=
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            ARCH_OPTION="-DNO_ARCH_NATIVE=ON"
        fi
        ;;
    *)
        SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DSECUREJOIN_ENABLE_SSE=OFF"
        ARCH_OPTION=
        ;;
esac

# Use the libOTe prefix from _setup_libote.sh
libote_prefix="$(cd .. && pwd)/libOTe-install"
if [[ ! -f "${libote_prefix}/lib/cmake/libOTe/libOTeConfig.cmake" && ! -f "${libote_prefix}/lib64/cmake/libOTe/libOTeConfig.cmake" ]]; then
    echo "libOTe install not found under ${libote_prefix}; run _setup_libote.sh first" >&2
    exit 1
fi

python3 build.py --noauto -DSODIUM_MONTGOMERY=false \
    -DCMAKE_PREFIX_PATH="${libote_prefix}" \
    --install=../secure-join-install \
    $SSE_OPTION $ARCH_OPTION

install_prefix="$(cd .. && pwd)/secure-join-install"
if [[ ! -f "${install_prefix}/lib/cmake/secureJoin/secureJoinConfig.cmake" && ! -f "${install_prefix}/lib64/cmake/secureJoin/secureJoinConfig.cmake" ]]; then
    echo "secureJoin build did not install secureJoinConfig.cmake under ${install_prefix}" >&2
    exit 1
fi

echo "==== SecureJoin installed"
