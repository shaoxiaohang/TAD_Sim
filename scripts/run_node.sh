#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

pushd $TADSIM_NODE_ADDON_DIR > /dev/null
info "start running node addon $@"
node test.js $@ > $TADSIM_DEV_LOG_DIR/$@.log
popd > /dev/null
