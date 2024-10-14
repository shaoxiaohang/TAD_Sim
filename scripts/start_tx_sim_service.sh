#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
source "${DIR}/base.sh"


$TADSIM_DEV_SERVICE_DIR/txSimService --service_dir=$TADSIM_DEV_SERVICE_DIR/scenario_service --scenario_dir=$HOME/.config/tadsim/sys/scenario --app_dir=$TADSIM_APP_DIR --log_dir=$TADSIM_LOG_DIR/dev

#/saturnv/build/release/linux-unpacked/resources/app/service/txSimService --service_dir=/saturnv/build/release/linux-unpacked/resources/app/service/scenario_service --scenario_dir=/home/dpx/.config/tadsim/sys/scenario --app_dir=/saturnv/build/release/linux-unpacked/resources/app

#/opt/tadsim/resources/app/service/txSimService --service_dir=/opt/tadsim/resources/app/service/scenario_service --scenario_dir=/home/dpx/.config/tadsim/sys/scenario --app_dir=/opt/tadsim/resources/app -log_dir=/saturnv/log