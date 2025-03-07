#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"

SERVICE_DIR=/saturnv/build/release/linux-unpacked/resources/app/service
FRAMEWORK_DIR=/saturnv/simcore/framework
REMOTE_SERVICE_DIR=xiaohang.shao@10.34.8.71:/home/users/xiaohang.shao/workspace/app-0215-sim/tadsim/linux-unpacked/resources/app/service


scp -r $SERVICE_DIR/traffic/txSimTraffic $REMOTE_SERVICE_DIR/traffic

# scp $FRAMEWORK_DIR/build/bin/txsim-local-service  $REMOTE_SERVICE_DIR
# scp $FRAMEWORK_DIR/build/bin/txsim-module-launcher $REMOTE_SERVICE_DIR
# scp $FRAMEWORK_DIR/src/node_addon/build/Release/txsim-play-service.node $REMOTE_SERVICE_DIR
