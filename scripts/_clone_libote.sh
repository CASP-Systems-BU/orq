#!/bin/bash

mkdir -p libOTe-install

if ! cd libOTe; then
    # Feb 2025: switching to master branch
    echo "NOTE: cloning from fork."
    git clone https://github.com/elimbaum/libOTe.git
    cd libOTe
    # Latest commit from master branch
    # This file needs to change with every new commit to trigger a new build on GitHub CI
    git checkout 587b325d2dfcab27a70f7954adc99a5cd65712d8
    git submodule update --init
fi