#!/bin/bash

mkdir -p libOTe-install

if ! cd libOTe; then
    # Feb 2025: switching to master branch
    echo "NOTE: cloning from fork."
    git clone https://github.com/osu-crypto/libOTe.git
    cd libOTe
    # Latest commit from master branch
    # This file needs to change with every new commit to trigger a new build on GitHub CI
    git checkout 97739a89fb7fb268221ab5c1628dc6fb8647c645
    git submodule update --init
fi