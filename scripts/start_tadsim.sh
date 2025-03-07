#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

info "$TADSIM_BUILD_DIR/release/linux-unpacked/tadsim"
$TADSIM_BUILD_DIR/release/linux-unpacked/tadsim