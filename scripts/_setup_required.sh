#!/bin/bash

set -e

sudo apt update
sudo apt install -y git cmake pkg-config build-essential manpages-dev gfortran \
        wget libsqlite3-0 libsqlite3-dev libsodium23 libsodium-dev \
        libopenmpi3 libopenmpi-dev openmpi-bin openmpi-common \
        python3 python3-pip libtool autoconf automake

STARTMPC_PATH="$(realpath "$(dirname "$0")/../include/backend/nocopy_communicator/startmpc/startmpc")"
mkdir ~/bin
sudo ln -sf $STARTMPC_PATH ~/bin/

# to disable, change `always` to `madvise`
sudo sh -c 'echo always > /sys/kernel/mm/transparent_hugepage/enabled'

# Add ~/bin to PATH if not there
if [[ ":$PATH:" != *":$HOME/bin:"* ]]; then
    echo "Adding ~/bin to PATH..."
    PATH="$HOME/bin:$PATH"
    export PATH
    
    printf '\n# Add ~/bin to PATH\nexport PATH="$HOME/bin:$PATH"\n' >> "$HOME/.bashrc"
fi
