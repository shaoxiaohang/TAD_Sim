#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

pushd ./simcore/map_server/hadmap_server/build/bin/ > /dev/null
info "start running test $1"
./scene_wrapper_tst --gtest_filter=$1
#./tests --gtest_filter=$1 > $TADSIM_TEST_LOG_DIR/$1.log
ok "finish running test $1"
popd > /dev/null