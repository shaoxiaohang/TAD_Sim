#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

info "$TADSIM_BUILD_DIR/release/linux-unpacked/tadsim"
$TADSIM_BUILD_DIR/release/linux-unpacked/tadsim