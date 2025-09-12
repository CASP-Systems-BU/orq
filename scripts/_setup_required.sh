#!/bin/bash

sudo apt update
sudo apt upgrade -y
sudo apt install -y git cmake pkg-config build-essential manpages-dev gfortran \
        wget libsqlite3-0 libsqlite3-dev libsodium23 libsodium-dev \
        libopenmpi3 libopenmpi-dev openmpi-bin openmpi-common \
        python3 python3-pip libtool autoconf automake

STARTMPC_PATH="$(realpath "$(dirname "$0")/../include/backend/nocopy_communicator/startmpc/startmpc")"
sudo ln -sf $STARTMPC_PATH /usr/local/bin/