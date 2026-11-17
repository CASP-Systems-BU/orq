#!/bin/bash

# abort if any command fails
set -e

## Set up libOTe
echo "==== Building libOTe..."

# Resolve before _clone_libote.sh, which cds into libOTe.
script_dir=$(cd "$(dirname "$0")" && pwd)
source "${script_dir}/_clone_libote.sh"

if [[ $(uname -m) == x86_64 ]]; then
    SSE_OPTION="-DENABLE_SSE=ON"
else
    # Disable features that are not available on ARM.
    SSE_OPTION="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DENABLE_SPIN=OFF"
fi

# macOS <termios.h> #defines baud-rate macros (B0, ...) that collide with
# C++ identifiers. Force-include a header that keeps B0 usable both as an
# identifier and as Asio's baud-rate constant. The compiler is launched from
# per-target directories under out/build, at different depths, so a relative
# -include path cannot resolve. Pass an absolute path with no space:
# build.py joins arguments into a shell command.
MAC_CXX_FLAGS=
if [[ "$(uname -s)" == Darwin ]]; then
    termios_header="${script_dir}/mac_undef_termios.h"
    if [[ ! -f "${termios_header}" ]]; then
        echo "missing macOS termios workaround header: ${termios_header}" >&2
        exit 1
    fi
    MAC_CXX_FLAGS="-DCMAKE_CXX_FLAGS=-include${termios_header}"
fi

python3 build.py --par=4 --boost --sodium --relic -DSODIUM_MONTGOMERY=false --all --install=../libOTe-install \
    $SSE_OPTION $MAC_CXX_FLAGS

echo "==== libOTe installed"
