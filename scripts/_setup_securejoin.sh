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
    # This is the version Sam fixed
    git checkout 86ca35dc81e086e5bbee15cb4eebab770afb5971
fi

case "$(uname -m)" in
    x86_64|amd64|AMD64)
        SSE_OPTION="-DENABLE_SSE=ON"
        ;;
    *)
        SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DSECUREJOIN_ENABLE_SSE=OFF"
        ;;
esac

# Link the libOTe already installed by _setup_libote.sh. --noauto is
# SecureJoin's switch for that. FindlibOTe.cmake is what its find_package
# call loads, and it publishes the libOTe target name SecureJoin links.
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

# install.cmake installs a camelCase secureJoin/ header directory before the
# secure-join/ sources. That directory is not in this checkout, and a missing
# DIRECTORY stops the install before the headers ORQ includes are copied.
# An empty directory satisfies the rule and copies nothing.
mkdir -p secureJoin

python3 build.py --noauto -DSODIUM_MONTGOMERY=false \
    -DLIBOTE_INSTALL_PREFIX="${libote_prefix}" \
    -DCMAKE_PREFIX_PATH="${libote_prefix}" \
    -DCMAKE_MODULE_PATH="${script_dir}/cmake" \
    --install=../secure-join-install \
    $SSE_OPTION $ARCH_OPTION

# build.py ignores cmake's exit status, so a failed configure still looks done.
install_prefix="../secure-join-install"
cfg=
for d in "${install_prefix}/lib/cmake/secureJoin" "${install_prefix}/lib64/cmake/secureJoin"; do
    if [[ -f "${d}/secureJoinConfig.cmake" ]]; then
        cfg="${d}"
    fi
done
header="${install_prefix}/include/secureJoin/secure-join/Prf/AltModPrf.h"
lib_ok=0
for d in "${install_prefix}/lib" "${install_prefix}/lib64"; do
    if [[ -f "${d}/libsecureJoin.a" || -f "${d}/libsecureJoin.so" ]]; then
        lib_ok=1
    fi
done
if [[ -z "${cfg}" || ! -f "${header}" || "${lib_ok}" -ne 1 ]]; then
    echo "SecureJoin install did not produce the package, library, or headers under ${install_prefix}" >&2
    exit 1
fi

echo "==== SecureJoin installed"
