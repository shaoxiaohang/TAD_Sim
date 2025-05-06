#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

info "Starting txsim-local-service..."
info "TADSIM_CONFIG_DIR: $TADSIM_CONFIG_DIR"
info "TADSIM_COORDINATOR_LOG_DIR: $TADSIM_COORDINATOR_LOG_DIR"

$TADSIM_DEV_SERVICE_DIR/txsim-local-service --root=$TADSIM_CONFIG_DIR --logdir=$TADSIM_COORDINATOR_LOG_DIR