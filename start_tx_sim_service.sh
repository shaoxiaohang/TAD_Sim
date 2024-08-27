#!/bin/bash

export LD_LIBRARY_PATH=./build/service/simdeps

/saturnv/simcore/map_server/build/bin/txSimService --service_dir=/saturnv/build/release/linux-unpacked/resources/app/service/scenario_service --scenario_dir=/home/dpx/.config/tadsim/sys/scenario --app_dir=/saturnv/build/release/linux-unpacked/resources/app

#/saturnv/build/release/linux-unpacked/resources/app/service/txSimService --service_dir=/saturnv/build/release/linux-unpacked/resources/app/service/scenario_service --scenario_dir=/home/dpx/.config/tadsim/sys/scenario --app_dir=/saturnv/build/release/linux-unpacked/resources/app

#/opt/tadsim/resources/app/service/txSimService --service_dir=/opt/tadsim/resources/app/service/scenario_service --scenario_dir=/home/dpx/.config/tadsim/sys/scenario --app_dir=/opt/tadsim/resources/app -log_dir=/saturnv/log