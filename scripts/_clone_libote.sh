#!/bin/bash

LIBOTE_REPO="https://github.com/osu-crypto/libOTe.git"
# This file needs to change with every new commit to trigger a new build on GitHub CI
LIBOTE_COMMIT="c1796737a91e5142b22ad7564f22bed1d9f2d957"

mkdir -p libOTe-install

if [ ! -d libOTe ]; then
    git clone "$LIBOTE_REPO" libOTe
fi
cd libOTe

# This script previously cloned a fork; retarget origin if needed.
if ! git remote get-url origin | grep -q 'osu-crypto/libOTe'; then
    git remote set-url origin "$LIBOTE_REPO"
fi
git fetch origin
git checkout "$LIBOTE_COMMIT"
git submodule update --init
