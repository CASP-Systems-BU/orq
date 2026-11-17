#!/bin/bash

# abort if any command fails
set -e

## Set up libOTe
echo "==== Building libOTe..."

# Resolve before _clone_libote.sh, which cds into libOTe.
script_dir=$(cd "$(dirname "$0")" && pwd)
source "${script_dir}/_clone_libote.sh"

case "$(uname -m)" in
    x86_64|amd64|AMD64)
        SSE_OPTION="-DENABLE_SSE=ON"
        # Do not cache an AVX-512 libOTe and run it on an AVX2-only runner.
        # Leave host detection alone on other x86 machines.
        AVX512_FLAG=
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            AVX512_FLAG="-DENABLE_AVX512=OFF"
        fi
        ;;
    *)
        # SSE, AVX, and AVX-512 are x86. ARM builds use the scalar paths.
        SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DENABLE_AVX512=OFF -DENABLE_SPIN=OFF"
        AVX512_FLAG=
        ;;
esac

# <termios.h> #defines B0 as the baud rate 0. macOS has always done this, and
# glibc does too (bits/termios.h). libOTe tests declare variables named B0
# after Boost.Asio has included termios.h, so the preprocessor turns B0(n)
# into a numeric constant. Force-include a header that keeps B0 usable both
# as an identifier and as Asio's baud-rate constant. The compiler is launched
# from per-target directories under out/build, at different depths, so a
# relative -include path cannot resolve. Pass an absolute path with no space:
# build.py joins arguments into a shell command.
termios_header="${script_dir}/mac_undef_termios.h"
if [[ ! -f "${termios_header}" ]]; then
    echo "missing termios workaround header: ${termios_header}" >&2
    exit 1
fi
TERMIOS_CXX_FLAGS="-DCMAKE_CXX_FLAGS=-include${termios_header}"

python3 build.py --par=4 --boost --sodium --relic -DSODIUM_MONTGOMERY=false --all --install=../libOTe-install \
    $SSE_OPTION $TERMIOS_CXX_FLAGS $AVX512_FLAG

echo "==== libOTe installed"
