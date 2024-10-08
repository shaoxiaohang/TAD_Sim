#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

$TADSIM_DEV_SERVICE_DIR/txsim-local-service --root=$TADSIM_CONFIG_DIR --logdir=$TADSIM_CONFIG_DIR/cache/debug_log