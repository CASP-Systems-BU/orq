#!/bin/bash

scripts_dir=$(dirname "$0")

$scripts_dir/_setup_required.sh
$scripts_dir/_setup_cryptotools.sh
$scripts_dir/_setup_ntl.sh
$scripts_dir/_setup_libote.sh
$scripts_dir/_setup_securejoin.sh
$scripts_dir/_setup_blaze.sh
